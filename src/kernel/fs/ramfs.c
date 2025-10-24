#include "ramfs.h"

#include "fs/dentry.h"
#include "fs/mount.h"
#include "fs/namespace.h"
#include "fs/path.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sync/lock.h"
#include "sync/mutex.h"
#include "sysfs.h"
#include "utils/ref.h"
#include "vfs.h"

#include <_internal/ERR.h>
#include <boot/boot_info.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

static mount_t* mount = NULL;

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
    list_remove(&superData->dentrys, &dentryData->entry);
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

static uint64_t ramfs_lookup(inode_t* dir, dentry_t* target)
{
    (void)dir;    // Unused
    (void)target; // Unused

    // All ramfs dentrys should always be in the cache, if lookup is called then the file/dir does not exist.
    return 0;
}

static uint64_t ramfs_create(inode_t* dir, dentry_t* target, path_flags_t flags)
{
    inode_t* newInode = ramfs_inode_new(dir->superblock, flags & PATH_DIRECTORY ? INODE_DIR : INODE_FILE, NULL, 0);
    if (newInode == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(newInode);

    if (ramfs_dentry_init(target) == ERR)
    {
        return ERR;
    }

    dentry_make_positive(target, newInode);

    return 0;
}

static void ramfs_truncate(inode_t* inode)
{
    if (inode->private != NULL)
    {
        heap_free(inode->private);
        inode->private = NULL;
    }
    inode->size = 0;
}

static uint64_t ramfs_link(dentry_t* old, inode_t* dir, dentry_t* target)
{
    (void)dir; // Unused

    if (ramfs_dentry_init(target) == ERR)
    {
        return ERR;
    }

    old->inode->linkCount++;
    dentry_make_positive(target, old->inode);

    return 0;
}

static uint64_t ramfs_remove_file(inode_t* parent, dentry_t* target)
{
    (void)parent; // Unused

    target->inode->linkCount--;
    ramfs_dentry_deinit(target);
    return 0;
}

static uint64_t ramfs_remove_directory(inode_t* parent, dentry_t* target)
{
    (void)parent; // Unused

    ramfs_dentry_deinit(target);
    return 0;
}

static uint64_t ramfs_remove(inode_t* parent, dentry_t* target, path_flags_t flags)
{
    if (target->inode->type == INODE_FILE)
    {
        ramfs_remove_file(parent, target);
    }
    else if (target->inode->type == INODE_DIR)
    {
        if (flags & PATH_RECURSIVE)
        {
            dentry_t* temp = NULL;
            dentry_t* child = NULL;
            LIST_FOR_EACH_SAFE(child, temp, &target->children, siblingEntry)
            {
                REF(child);
                ramfs_remove(target->inode, child, flags);
                DEREF(child);
            }
        }
        else
        {
            if (!list_is_empty(&target->children))
            {
                errno = ENOTEMPTY;
                return ERR;
            }
        }
        ramfs_remove_directory(parent, target);
    }
    return 0;
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
    .remove = ramfs_remove,
    .cleanup = ramfs_inode_cleanup,
};

static dentry_ops_t dentryOps = {
    .getdents = dentry_generic_getdents,
};

static void ramfs_superblock_cleanup(superblock_t* superblock)
{
    (void)superblock; // Unused

    panic(NULL, "ramfs unmounted\n");
}

static superblock_ops_t superOps = {
    .cleanup = ramfs_superblock_cleanup,
};

static dentry_t* ramfs_load_file(superblock_t* superblock, dentry_t* parent, const char* name, const boot_file_t* in)
{
    dentry_t* dentry = dentry_new(superblock, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create ramfs file dentry");
    }
    DEREF_DEFER(dentry);

    if (ramfs_dentry_init(dentry) == ERR)
    {
        panic(NULL, "Failed to initialize ramfs dentry");
    }

    inode_t* inode = ramfs_inode_new(superblock, INODE_FILE, in->data, in->size);
    if (inode == NULL)
    {
        panic(NULL, "Failed to create ramfs file inode");
    }
    DEREF_DEFER(inode);

    if (dentry_make_positive(dentry, inode) == ERR)
    {
        panic(NULL, "Failed to make ramfs file dentry positive");
    }

    return REF(dentry);
}

static dentry_t* ramfs_load_dir(superblock_t* superblock, dentry_t* parent, const char* name, const boot_dir_t* in)
{
    ramfs_superblock_data_t* superData = superblock->private;

    dentry_t* dentry = dentry_new(superblock, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create ramfs dentry");
    }
    DEREF_DEFER(dentry);

    if (ramfs_dentry_init(dentry) == ERR)
    {
        panic(NULL, "Failed to initialize ramfs dentry");
    }

    inode_t* inode = ramfs_inode_new(superblock, INODE_DIR, NULL, 0);
    if (inode == NULL)
    {
        panic(NULL, "Failed to create ramfs inode");
    }
    DEREF_DEFER(inode);

    if (dentry_make_positive(dentry, inode) == ERR)
    {
        panic(NULL, "Failed to make ramfs dentry positive");
    }

    boot_file_t* file;
    LIST_FOR_EACH(file, &in->files, entry)
    {
        DEREF(ramfs_load_file(superblock, dentry, file->name, file));
    }

    boot_dir_t* child;
    LIST_FOR_EACH(child, &in->children, entry)
    {
        DEREF(ramfs_load_dir(superblock, dentry, child->name, child));
    }

    return REF(dentry);
}

static dentry_t* ramfs_mount(filesystem_t* fs, const char* devName, void* private)
{
    (void)devName; // Unused

    superblock_t* superblock = superblock_new(fs, VFS_DEVICE_NAME_NONE, &superOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(superblock);

    superblock->blockSize = 0;
    superblock->maxFileSize = UINT64_MAX;

    ramfs_superblock_data_t* data = heap_alloc(sizeof(ramfs_superblock_data_t), HEAP_NONE);
    if (data == NULL)
    {
        return NULL;
    }
    list_init(&data->dentrys);
    lock_init(&data->lock);
    superblock->private = data;

    boot_disk_t* disk = private;

    superblock->root = ramfs_load_dir(superblock, NULL, VFS_ROOT_ENTRY_NAME, disk->root);
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
    DEREF_DEFER(inode);

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

void ramfs_init(const boot_disk_t* disk)
{
    LOG_INFO("registering ramfs\n");
    if (vfs_register_fs(&ramfs) == ERR)
    {
        panic(NULL, "Failed to register ramfs");
    }
    LOG_INFO("mounting ramfs\n");
    mount = namespace_mount(NULL, NULL, VFS_DEVICE_NAME_NONE, RAMFS_NAME, (void*)disk);
    if (mount == NULL)
    {
        panic(NULL, "Failed to mount ramfs");
    }
    LOG_INFO("ramfs initialized\n");
}
