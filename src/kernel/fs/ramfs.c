#include "ramfs.h"

#include "fs/dentry.h"
#include "fs/mount.h"
#include "fs/path.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sync/lock.h"
#include "sync/mutex.h"
#include "sysfs.h"
#include "utils/ref.h"
#include "vfs.h"

#include <boot/boot_info.h>

#include <assert.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

static _Atomic(inode_number_t) newNumber = ATOMIC_VAR_INIT(1);

static inode_t* ramfs_inode_new(superblock_t* superblock, inode_type_t type, void* buffer, uint64_t size);

static uint64_t ramfs_dentry_init(dentry_t* dentry)
{
    ramfs_superblock_data_t* superData = dentry->superblock->private;

    ramfs_dentry_data_t* dentryData = heap_alloc(sizeof(ramfs_dentry_data_t), HEAP_NONE);
    if (dentryData == NULL)
    {
        return ERR;
    }
    list_entry_init(&dentryData->entry);
    dentryData->dentry = REF(dentry);
    dentry->private = dentryData;

    lock_acquire(&superData->lock);
    list_push(&superData->dentrys, &dentryData->entry);
    lock_release(&superData->lock);
    return 0;
}

static void ramfs_dentry_deinit(dentry_t* dentry)
{
    ramfs_superblock_data_t* superData = dentry->superblock->private;

    ramfs_dentry_data_t* dentryData = dentry->private;
    assert(dentryData != NULL);

    DEREF(dentryData->dentry);
    dentryData->dentry = NULL;

    lock_acquire(&superData->lock);
    list_remove(&dentryData->entry);
    lock_release(&superData->lock);

    heap_free(dentryData);
}

static uint64_t ramfs_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    MUTEX_SCOPE(&file->inode->mutex);

    if (file->inode->private == NULL)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, file->inode->private, file->inode->size);
}

static uint64_t ramfs_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    MUTEX_SCOPE(&file->inode->mutex);

    if (file->flags & PATH_APPEND)
    {
        *offset = file->inode->size;
    }

    if (*offset + count > file->inode->size)
    {
        void* newData = heap_realloc(file->inode->private, *offset + count, HEAP_VMM);
        if (newData == NULL)
        {
            return ERR;
        }
        memset(newData + file->inode->size, 0, *offset + count - file->inode->size);
        file->inode->private = newData;
        file->inode->size = *offset + count;
    }

    memcpy(file->inode->private + *offset, buffer, count);
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
    // All ramfs dentrys should always be in the cache, if lookup is called then the file/dir does not exist.
    return LOOKUP_NO_ENTRY;
}

static uint64_t ramfs_create(inode_t* dir, dentry_t* target, path_flags_t flags)
{
    MUTEX_SCOPE(&dir->mutex);

    if (dir->type != INODE_DIR)
    {
        LOG_ERR("ramfs_create called using a non-directory inode.\n");
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

    inode_t* newInode =
        ramfs_inode_new(dir->superblock, flags & PATH_DIRECTORY ? INODE_DIR : INODE_FILE, NULL, 0);
    if (newInode == NULL)
    {
        return ERR;
    }
    REF_DEFER(newInode);

    if (ramfs_dentry_init(target) == ERR)
    {
        return ERR;
    }

    dentry_make_positive(target, newInode);

    return 0;
}

static void ramfs_truncate(inode_t* inode)
{
    MUTEX_SCOPE(&inode->mutex);

    if (inode->type != INODE_FILE)
    {
        LOG_ERR("ramfs_truncate called using a non-file inode.\n");
        return;
    }

    if (inode->private != NULL)
    {
        heap_free(inode->private);
        inode->private = NULL;
        inode->size = 0;
    }
}

static inode_t* ramfs_link(dentry_t* old, inode_t* newParent, const char* name)
{
    errno = ENOSYS;
    return NULL;
}

static uint64_t ramfs_unlink(inode_t* parent, dentry_t* target)
{
    inode_t* inode = REF(target->inode);
    REF_DEFER(inode);

    MUTEX_SCOPE(&inode->mutex);

    inode->linkCount--;
    ramfs_dentry_deinit(target);
    return 0;
}

static uint64_t ramfs_rmdir(inode_t* parent, dentry_t* target)
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
    if (inode->private != NULL)
    {
        heap_free(inode->private);
    }
}

static inode_ops_t inodeOps = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .truncate = ramfs_truncate,
    .link = ramfs_link,
    .unlink = ramfs_unlink,
    .rmdir = ramfs_rmdir,
    .rename = ramfs_rename,
    .cleanup = ramfs_inode_cleanup,
};

static dentry_ops_t dentryOps = {
    .getdents = dentry_generic_getdents,
};

static void ramfs_superblock_cleanup(superblock_t* superblock)
{
    panic(NULL, "ramfs unmounted\n");
}

static superblock_ops_t superOps = {
    .cleanup = ramfs_superblock_cleanup,
};

static dentry_t* ramfs_load_file(superblock_t* superblock, dentry_t* parent, const char* name, const ram_file_t* in)
{
    dentry_t* dentry = dentry_new(superblock, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create ramfs file dentry");
    }
    REF_DEFER(dentry);

    if (ramfs_dentry_init(dentry) == ERR)
    {
        panic(NULL, "Failed to initialize ramfs dentry");
    }

    inode_t* inode = ramfs_inode_new(superblock, INODE_FILE, in->data, in->size);
    if (inode == NULL)
    {
        panic(NULL, "Failed to create ramfs file inode");
    }
    REF_DEFER(inode);

    dentry_make_positive(dentry, inode);
    vfs_add_dentry(dentry);

    return REF(dentry);
}

static dentry_t* ramfs_load_dir(superblock_t* superblock, dentry_t* parent, const char* name, const ram_dir_t* in)
{
    ramfs_superblock_data_t* superData = superblock->private;

    dentry_t* dentry = dentry_new(superblock, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create ramfs dentry");
    }
    REF_DEFER(dentry);

    if (ramfs_dentry_init(dentry) == ERR)
    {
        panic(NULL, "Failed to initialize ramfs dentry");
    }

    inode_t* inode = ramfs_inode_new(superblock, INODE_DIR, NULL, 0);
    if (inode == NULL)
    {
        panic(NULL, "Failed to create ramfs inode");
    }
    REF_DEFER(inode);

    dentry_make_positive(dentry, inode);
    vfs_add_dentry(dentry);

    node_t* child;
    LIST_FOR_EACH(child, &in->node.children, entry)
    {
        if (child->type == RAMFS_DIR)
        {
            ram_dir_t* dir = CONTAINER_OF(child, ram_dir_t, node);

            DEREF(ramfs_load_dir(superblock, dentry, dir->node.name, dir));
        }
        else if (child->type == RAMFS_FILE)
        {
            ram_file_t* file = CONTAINER_OF(child, ram_file_t, node);

            DEREF(ramfs_load_file(superblock, dentry, file->node.name, file));
        }
    }

    return REF(dentry);
}

static dentry_t* ramfs_mount(filesystem_t* fs, superblock_flags_t flags, const char* devName, void* private)
{
    superblock_t* superblock = superblock_new(fs, VFS_DEVICE_NAME_NONE, &superOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    REF_DEFER(superblock);

    superblock->blockSize = 0;
    superblock->maxFileSize = UINT64_MAX;
    superblock->flags = flags;

    ramfs_superblock_data_t* data = heap_alloc(sizeof(ramfs_superblock_data_t), HEAP_NONE);
    if (data == NULL)
    {
        return NULL;
    }
    list_init(&data->dentrys);
    lock_init(&data->lock);
    superblock->private = data;

    superblock->root = ramfs_load_dir(superblock, NULL, VFS_ROOT_ENTRY_NAME, private);
    if (superblock->root == NULL)
    {
        return NULL;
    }

    return REF(superblock->root);
}

static inode_t* ramfs_inode_new(superblock_t* superblock, inode_type_t type, void* buffer, uint64_t size)
{
    inode_t* inode = inode_new(superblock, atomic_fetch_add(&newNumber, 1), type, &inodeOps, &fileOps);
    if (inode == NULL)
    {
        return NULL;
    }
    REF_DEFER(inode);

    inode->blocks = 0;
    inode->size = size;

    if (buffer != NULL)
    {
        inode->private = heap_alloc(size, HEAP_VMM);
        if (inode->private == NULL)
        {
            return NULL;
        }
        memcpy(inode->private, buffer, size);
    }
    else
    {
        inode->private = NULL;
        inode->size = 0;
    }

    return REF(inode);
}

static filesystem_t ramfs = {
    .name = RAMFS_NAME,
    .mount = ramfs_mount,
};

void ramfs_init(ram_disk_t* disk)
{
    LOG_INFO("registering ramfs\n");
    if (vfs_register_fs(&ramfs) == ERR)
    {
        panic(NULL, "Failed to register ramfs");
    }
    LOG_INFO("mounting ramfs\n");
    if (vfs_mount(VFS_DEVICE_NAME_NONE, NULL, RAMFS_NAME, SUPER_NONE, disk->root) == ERR)
    {
        panic(NULL, "Failed to mount ramfs");
    }
    LOG_INFO("ramfs initialized\n");
}
