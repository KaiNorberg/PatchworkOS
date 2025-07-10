#include "ramfs.h"

#include "fs/dentry.h"
#include "fs/mount.h"
#include "fs/path.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sync/lock.h"
#include "sysfs.h"
#include "vfs.h"

#include <boot/boot_info.h>

#include <assert.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

static ramfs_inode_t* root;

static _Atomic(inode_number_t) newNumber = ATOMIC_VAR_INIT(0);

static ramfs_inode_t* ramfs_inode_new(superblock_t* superblock, inode_type_t type, void* data, uint64_t size);

static uint64_t ramfs_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    ramfs_inode_t* ramfsInode = CONTAINER_OF(file->inode, ramfs_inode_t, inode);
    LOCK_DEFER(&ramfsInode->inode.lock);

    if (ramfsInode->data == NULL)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, ramfsInode->data, ramfsInode->inode.size);
}

static uint64_t ramfs_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    ramfs_inode_t* ramfsInode = CONTAINER_OF(file->inode, ramfs_inode_t, inode);
    LOCK_DEFER(&ramfsInode->inode.lock);

    if (file->flags & PATH_APPEND)
    {
        *offset = ramfsInode->inode.size;
    }

    if (*offset + count > ramfsInode->inode.size)
    {
        void* newData = heap_realloc(ramfsInode->data, *offset + count, HEAP_VMM);
        if (newData == NULL)
        {
            return ERR;
        }
        memset(newData + ramfsInode->inode.size, 0, *offset + count - ramfsInode->inode.size);
        ramfsInode->data = newData;
        ramfsInode->inode.size = *offset + count;
    }

    memcpy(ramfsInode->data + *offset, buffer, count);
    *offset += count;
    return count;
}

static file_ops_t fileOps = {
    .read = ramfs_read,
    .write = ramfs_write,
    .seek = file_generic_seek,
};

static lookup_result_t ramfs_lookup(inode_t* dir, dentry_t* target)
{
    // All ramfs entires should always be in the cache, if lookup is called then the file/dir does not exist.
    return LOOKUP_NO_ENTRY;
}

static uint64_t ramfs_create(inode_t* dir, dentry_t* target, path_flags_t flags)
{
    ramfs_inode_t* inode = CONTAINER_OF(dir, ramfs_inode_t, inode);
    LOCK_DEFER(&inode->inode.lock);
    LOCK_DEFER(&target->lock);

    if (inode->inode.type != INODE_DIR)
    {
        LOG_ERR("ramfs_create: called using a non-directory inode.\n");
        errno = EINVAL;
        return ERR;
    }

    if (!(target->flags & DENTRY_NEGATIVE))
    {
        if (flags & PATH_EXCLUSIVE)
        {
            errno = EEXIST;
            return ERR;
        }

        return 0;
    }

    ramfs_inode_t* newInode = ramfs_inode_new(inode->inode.superblock, flags & PATH_DIRECTORY ? INODE_DIR : INODE_FILE, NULL,
    0);
    if (newInode == NULL)
    {
        return ERR;
    }
    INODE_DEFER(&newInode->inode);

    dentry_make_positive(target, &newInode->inode);
    return 0;
}

static void ramfs_truncate(inode_t* inode)
{
    LOCK_DEFER(&inode->lock);

    if (inode->type != INODE_FILE)
    {
        LOG_ERR("ramfs_truncate: called using a non-file inode.\n");
        return;
    }

    ramfs_inode_t* ramfsInode = CONTAINER_OF(inode, ramfs_inode_t, inode);

    heap_free(ramfsInode->data);
    ramfsInode->data = NULL;
    ramfsInode->inode.size = 0;
}

static inode_t* ramfs_link(dentry_t* old, inode_t* newParent, const char* name)
{
    errno = ENOSYS;
    return NULL;
}

static uint64_t ramfs_remove(inode_t* parent, dentry_t* target)
{
    errno = ENOSYS;
    return ERR;
}

static inode_t* ramfs_rename(inode_t* oldParent, dentry_t* old, inode_t* newParent, const char* name)
{
    errno = ENOSYS;
    return NULL;
}

static void ramfs_inode_cleanup(inode_t* inode)
{
    ramfs_inode_t* ramfsInode = CONTAINER_OF(inode, ramfs_inode_t, inode);

    if (ramfsInode->data != NULL)
    {
        heap_free(ramfsInode->data);
    }
}

static inode_ops_t inodeOps = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .truncate = ramfs_truncate,
    .link = ramfs_link,
    .remove = ramfs_remove,
    .rename = ramfs_rename,
    .cleanup = ramfs_inode_cleanup,
};

static dentry_ops_t dentryOps = {
    .getdirent = dentry_generic_getdirent,
};

static inode_t* ramfs_alloc_inode(superblock_t* superblock)
{
    return heap_alloc(sizeof(ramfs_inode_t), HEAP_NONE);
}

static void ramfs_free_inode(superblock_t* superblock, inode_t* inode)
{
    heap_free(CONTAINER_OF(inode, ramfs_inode_t, inode));
}

static void ramfs_superblock_cleanup(superblock_t* superblock)
{
    panic(NULL, "ramfs unmounted\n");
}

static superblock_ops_t superOps = {
    .allocInode = ramfs_alloc_inode,
    .freeInode = ramfs_free_inode,
    .cleanup = ramfs_superblock_cleanup,
};

static dentry_t* ramfs_load_dir(superblock_t* superblock, dentry_t* parent, const char* name, const ram_dir_t* in)
{
    // We dont dereference the dentries such that they stay in memory.

    dentry_t* dentry = dentry_new(superblock, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create ramfs dentry");
    }

    ramfs_inode_t* inode = ramfs_inode_new(superblock, INODE_DIR, NULL, 0);
    if (inode == NULL)
    {
        panic(NULL, "Failed to create ramfs inode");
    }

    dentry_make_positive(dentry, &inode->inode);
    vfs_add_dentry(dentry);

    node_t* child;
    LIST_FOR_EACH(child, &in->node.children, entry)
    {
        if (child->type == RAMFS_DIR)
        {
            ram_dir_t* dir = CONTAINER_OF(child, ram_dir_t, node);

            ramfs_load_dir(superblock, dentry, dir->node.name, dir);
        }
        else if (child->type == RAMFS_FILE)
        {
            ram_file_t* file = CONTAINER_OF(child, ram_file_t, node);

            dentry_t* fileDentry = dentry_new(superblock, dentry, file->node.name);
            if (fileDentry == NULL)
            {
                panic(NULL, "Failed to create ramfs file dentry");
            }

            ramfs_inode_t* fileInode = ramfs_inode_new(superblock, INODE_FILE, file->data, file->size);
            if (fileInode == NULL)
            {
                panic(NULL, "Failed to create ramfs file inode");
            }

            dentry_make_positive(fileDentry, &fileInode->inode);
            vfs_add_dentry(fileDentry);
        }
    }

    return dentry;
}

static dentry_t* ramfs_mount(filesystem_t* fs, superblock_flags_t flags, const char* devName, void* private)
{
    superblock_t* superblock = superblock_new(fs, VFS_DEVICE_NAME_NONE, &superOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    SUPER_DEFER(superblock);

    superblock->blockSize = 0;
    superblock->maxFileSize = UINT64_MAX;
    superblock->flags = flags;

    ramfs_inode_t* inode = ramfs_inode_new(superblock, INODE_DIR, NULL, 0);
    if (inode == NULL)
    {
        return NULL;
    }
    INODE_DEFER(&inode->inode);

    superblock->root = ramfs_load_dir(superblock, NULL, VFS_ROOT_ENTRY_NAME, private);
    if (superblock->root == NULL)
    {
        return NULL;
    }

    return dentry_ref(superblock->root);
}

static filesystem_t ramfs = {
    .name = RAMFS_NAME,
    .mount = ramfs_mount,
};

static ramfs_inode_t* ramfs_inode_new(superblock_t* superblock, inode_type_t type, void* data, uint64_t size)
{
    // Becouse of the ramfs_alloc_inode function, this allocated inode is actually a ramfs_inode_t.
    inode_t* inode = inode_new(superblock, atomic_fetch_add(&newNumber, 1), type, &inodeOps, &fileOps);
    if (inode == NULL)
    {
        return NULL;
    }

    inode->blocks = 0;
    inode->size = size;

    ramfs_inode_t* ramfsInode = CONTAINER_OF(inode, ramfs_inode_t, inode);

    if (data == NULL)
    {
        assert(size == 0); // Sanity check.
        ramfsInode->data = NULL;
        return ramfsInode;
    }

    ramfsInode->data = heap_alloc(size, HEAP_VMM);
    if (ramfsInode->data == NULL)
    {
        inode_deref(&ramfsInode->inode);
        return NULL;
    }
    memcpy(ramfsInode->data, data, size);
    return ramfsInode;
}

void ramfs_init(ram_disk_t* disk)
{
    LOG_INFO("ramfs: init\n");

    if (vfs_register_fs(&ramfs) == ERR)
    {
        panic(NULL, "Failed to register ramfs");
    }
    if (vfs_mount(VFS_DEVICE_NAME_NONE, NULL, RAMFS_NAME, SUPER_NONE, disk->root) == ERR)
    {
        panic(NULL, "Failed to mount ramfs");
    }
}
