#include <kernel/fs/tmpfs.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/init/boot_info.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>
#include <kernel/utils/ref.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>

static bool initialized = false;

static inode_t* tmpfs_inode_new(superblock_t* superblock, inode_type_t type, void* buffer, uint64_t size);

static void tmpfs_dentry_add(dentry_t* dentry)
{
    tmpfs_superblock_data_t* super = dentry->superblock->private;

    lock_acquire(&super->lock);
    list_push_back(&super->dentrys, &dentry->otherEntry);
    REF(dentry);
    lock_release(&super->lock);
}

static void tmpfs_dentry_remove(dentry_t* dentry)
{
    tmpfs_superblock_data_t* super = dentry->superblock->private;

    lock_acquire(&super->lock);
    list_remove(&super->dentrys, &dentry->otherEntry);
    UNREF(dentry);
    lock_release(&super->lock);

    dentry_remove(dentry);
}

static uint64_t tmpfs_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    MUTEX_SCOPE(&file->inode->mutex);

    if (file->inode->private == NULL)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, file->inode->private, file->inode->size);
}

static uint64_t tmpfs_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    MUTEX_SCOPE(&file->inode->mutex);

    uint64_t requiredSize = *offset + count;
    if (requiredSize > file->inode->size)
    {
        void* newData = realloc(file->inode->private, requiredSize);
        if (newData == NULL)
        {
            return ERR;
        }
        memset(newData + file->inode->size, 0, requiredSize - file->inode->size);
        file->inode->private = newData;
        file->inode->size = requiredSize;
    }

    return BUFFER_WRITE(file->inode->private, count, offset, buffer, file->inode->size);
}

static file_ops_t fileOps = {
    .read = tmpfs_read,
    .write = tmpfs_write,
    .seek = file_generic_seek,
};

static uint64_t tmpfs_create(inode_t* dir, dentry_t* target, mode_t mode)
{
    MUTEX_SCOPE(&dir->mutex);

    inode_t* inode = tmpfs_inode_new(dir->superblock, mode & MODE_DIRECTORY ? INODE_DIR : INODE_FILE, NULL, 0);
    if (inode == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(inode);

    dentry_make_positive(target, inode);
    tmpfs_dentry_add(target);

    return 0;
}

static void tmpfs_truncate(inode_t* inode)
{
    MUTEX_SCOPE(&inode->mutex);

    if (inode->private != NULL)
    {
        free(inode->private);
        inode->private = NULL;
    }
    inode->size = 0;
}

static uint64_t tmpfs_link(inode_t* dir, dentry_t* old, dentry_t* target)
{
    MUTEX_SCOPE(&dir->mutex);

    dentry_make_positive(target, old->inode);
    tmpfs_dentry_add(target);

    return 0;
}

static uint64_t tmpfs_readlink(inode_t* inode, char* buffer, uint64_t count)
{
    MUTEX_SCOPE(&inode->mutex);

    if (inode->private == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t copySize = MIN(count, inode->size);
    memcpy(buffer, inode->private, copySize);
    return copySize;
}

static uint64_t tmpfs_symlink(inode_t* dir, dentry_t* target, const char* dest)
{
    MUTEX_SCOPE(&dir->mutex);

    inode_t* inode = tmpfs_inode_new(dir->superblock, INODE_SYMLINK, (void*)dest, strlen(dest));
    if (inode == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(inode);

    dentry_make_positive(target, inode);
    tmpfs_dentry_add(target);

    return 0;
}

static uint64_t tmpfs_remove(inode_t* dir, dentry_t* target)
{
    MUTEX_SCOPE(&dir->mutex);

    if (target->inode->type == INODE_FILE || target->inode->type == INODE_SYMLINK)
    {
        tmpfs_dentry_remove(target);
    }
    else if (target->inode->type == INODE_DIR)
    {
        if (!list_is_empty(&target->children))
        {
            errno = ENOTEMPTY;
            return ERR;
        }

        tmpfs_dentry_remove(target);
    }

    return 0;
}

static void tmpfs_inode_cleanup(inode_t* inode)
{
    if (inode->private != NULL)
    {
        free(inode->private);
        inode->private = NULL;
        inode->size = 0;
    }
}

static inode_ops_t inodeOps = {
    .create = tmpfs_create,
    .truncate = tmpfs_truncate,
    .link = tmpfs_link,
    .readlink = tmpfs_readlink,
    .symlink = tmpfs_symlink,
    .remove = tmpfs_remove,
    .cleanup = tmpfs_inode_cleanup,
};

static dentry_ops_t dentryOps = {
    .iterate = dentry_generic_iterate,
};

static void tmpfs_superblock_cleanup(superblock_t* superblock)
{
    (void)superblock; // Unused

    panic(NULL, "tmpfs unmounted\n");
}

static superblock_ops_t superOps = {
    .cleanup = tmpfs_superblock_cleanup,
};

static dentry_t* tmpfs_load_file(superblock_t* superblock, dentry_t* parent, const char* name, const boot_file_t* in)
{
    dentry_t* dentry = dentry_new(superblock, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create tmpfs file dentry");
    }
    UNREF_DEFER(dentry);

    tmpfs_dentry_add(dentry);

    inode_t* inode = tmpfs_inode_new(superblock, INODE_FILE, in->data, in->size);
    if (inode == NULL)
    {
        panic(NULL, "Failed to create tmpfs file inode");
    }
    UNREF_DEFER(inode);

    dentry_make_positive(dentry, inode);

    return REF(dentry);
}

static dentry_t* tmpfs_load_dir(superblock_t* superblock, dentry_t* parent, const char* name, const boot_dir_t* in)
{
    tmpfs_superblock_data_t* superData = superblock->private;

    dentry_t* dentry = dentry_new(superblock, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create tmpfs dentry");
    }
    UNREF_DEFER(dentry);

    inode_t* inode = tmpfs_inode_new(superblock, INODE_DIR, NULL, 0);
    if (inode == NULL)
    {
        panic(NULL, "Failed to create tmpfs inode");
    }
    UNREF_DEFER(inode);

    tmpfs_dentry_add(dentry);
    dentry_make_positive(dentry, inode);

    boot_file_t* file;
    LIST_FOR_EACH(file, &in->files, entry)
    {
        UNREF(tmpfs_load_file(superblock, dentry, file->name, file));
    }

    boot_dir_t* child;
    LIST_FOR_EACH(child, &in->children, entry)
    {
        UNREF(tmpfs_load_dir(superblock, dentry, child->name, child));
    }

    return REF(dentry);
}

static dentry_t* tmpfs_mount(filesystem_t* fs, const char* devName, void* private)
{
    (void)devName; // Unused
    (void)private; // Unused

    superblock_t* superblock = superblock_new(fs, VFS_DEVICE_NAME_NONE, &superOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(superblock);

    superblock->blockSize = 0;
    superblock->maxFileSize = UINT64_MAX;

    tmpfs_superblock_data_t* data = malloc(sizeof(tmpfs_superblock_data_t));
    if (data == NULL)
    {
        return NULL;
    }
    list_init(&data->dentrys);
    lock_init(&data->lock);
    superblock->private = data;

    if (!initialized)
    {
        boot_info_t* bootInfo = boot_info_get();
        const boot_disk_t* disk = &bootInfo->disk;

        dentry_t* root = tmpfs_load_dir(superblock, NULL, VFS_ROOT_ENTRY_NAME, disk->root);
        if (root == NULL)
        {
            return NULL;
        }

        superblock->root = root;
        return REF(superblock->root);
    }

    dentry_t* dentry = dentry_new(superblock, NULL, VFS_ROOT_ENTRY_NAME);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    inode_t* inode = tmpfs_inode_new(superblock, INODE_DIR, NULL, 0);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);

    tmpfs_dentry_add(dentry);
    dentry_make_positive(dentry, inode);

    superblock->root = dentry;
    return REF(superblock->root);
}

static inode_t* tmpfs_inode_new(superblock_t* superblock, inode_type_t type, void* buffer, uint64_t size)
{
    inode_t* inode = inode_new(superblock, vfs_id_get(), type, &inodeOps, &fileOps);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);

    inode->blocks = 0;

    if (buffer != NULL)
    {
        inode->private = malloc(size);
        if (inode->private == NULL)
        {
            return NULL;
        }
        memcpy(inode->private, buffer, size);
        inode->size = size;
    }
    else
    {
        inode->private = NULL;
        inode->size = 0;
    }

    return REF(inode);
}

static filesystem_t tmpfs = {
    .name = TMPFS_NAME,
    .mount = tmpfs_mount,
};

void tmpfs_init(void)
{
    LOG_INFO("registering tmpfs\n");
    if (filesystem_register(&tmpfs) == ERR)
    {
        panic(NULL, "Failed to register tmpfs");
    }
    LOG_INFO("mounting tmpfs\n");

    process_t* process = sched_process();
    mount_t* temp = namespace_mount(&process->ns, VFS_DEVICE_NAME_NONE, TMPFS_NAME, NULL, MODE_CHILDREN | MODE_ALL_PERMS, NULL);
    if (temp == NULL)
    {
        panic(NULL, "Failed to mount tmpfs");
    }
    UNREF(temp);
    LOG_INFO("tmpfs initialized\n");

    initialized = true;
}
