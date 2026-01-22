#include <kernel/fs/tmpfs.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/vnode.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
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
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/math.h>

static bool initialized = false;

static vnode_t* tmpfs_vnode_new(superblock_t* superblock, vtype_t type, void* buffer, uint64_t size);

static void tmpfs_dentry_add(dentry_t* dentry)
{
    tmpfs_superblock_data_t* super = dentry->superblock->data;

    lock_acquire(&super->lock);
    list_push_back(&super->dentrys, &dentry->otherEntry);
    REF(dentry);
    lock_release(&super->lock);
}

static void tmpfs_dentry_remove(dentry_t* dentry)
{
    tmpfs_superblock_data_t* super = dentry->superblock->data;

    lock_acquire(&super->lock);
    list_remove(&dentry->otherEntry);
    UNREF(dentry);
    lock_release(&super->lock);

    dentry_remove(dentry);
}

static size_t tmpfs_read(file_t* file, void* buffer, size_t count, size_t* offset)
{
    MUTEX_SCOPE(&file->vnode->mutex);

    if (file->vnode->data == NULL)
    {
        return 0;
    }

    return BUFFER_READ(buffer, count, offset, file->vnode->data, file->vnode->size);
}

static size_t tmpfs_write(file_t* file, const void* buffer, size_t count, size_t* offset)
{
    MUTEX_SCOPE(&file->vnode->mutex);

    size_t requiredSize = *offset + count;
    if (requiredSize > file->vnode->size)
    {
        void* newData = realloc(file->vnode->data, requiredSize);
        if (newData == NULL)
        {
            return ERR;
        }
        memset(newData + file->vnode->size, 0, requiredSize - file->vnode->size);
        file->vnode->data = newData;
        file->vnode->size = requiredSize;
    }

    return BUFFER_WRITE(buffer, count, offset, file->vnode->data, file->vnode->size);
}

static file_ops_t fileOps = {
    .read = tmpfs_read,
    .write = tmpfs_write,
    .seek = file_generic_seek,
};

static uint64_t tmpfs_create(vnode_t* dir, dentry_t* target, mode_t mode)
{
    MUTEX_SCOPE(&dir->mutex);

    vnode_t* vnode = tmpfs_vnode_new(dir->superblock, mode & MODE_DIRECTORY ? VDIR : VREG, NULL, 0);
    if (vnode == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(vnode);

    dentry_make_positive(target, vnode);
    tmpfs_dentry_add(target);

    return 0;
}

static void tmpfs_truncate(vnode_t* vnode)
{
    MUTEX_SCOPE(&vnode->mutex);

    if (vnode->data != NULL)
    {
        free(vnode->data);
        vnode->data = NULL;
    }
    vnode->size = 0;
}

static uint64_t tmpfs_link(vnode_t* dir, dentry_t* old, dentry_t* target)
{
    MUTEX_SCOPE(&dir->mutex);

    dentry_make_positive(target, old->vnode);
    tmpfs_dentry_add(target);

    return 0;
}

static uint64_t tmpfs_readlink(vnode_t* vnode, char* buffer, uint64_t count)
{
    MUTEX_SCOPE(&vnode->mutex);

    if (vnode->data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t copySize = MIN(count, vnode->size);
    memcpy(buffer, vnode->data, copySize);
    return copySize;
}

static uint64_t tmpfs_symlink(vnode_t* dir, dentry_t* target, const char* dest)
{
    MUTEX_SCOPE(&dir->mutex);

    vnode_t* vnode = tmpfs_vnode_new(dir->superblock, VSYMLINK, (void*)dest, strlen(dest));
    if (vnode == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(vnode);

    dentry_make_positive(target, vnode);
    tmpfs_dentry_add(target);

    return 0;
}

static uint64_t tmpfs_remove(vnode_t* dir, dentry_t* target)
{
    MUTEX_SCOPE(&dir->mutex);

    if (target->vnode->type == VREG || target->vnode->type == VSYMLINK)
    {
        tmpfs_dentry_remove(target);
    }
    else if (target->vnode->type == VDIR)
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

static void tmpfs_vnode_cleanup(vnode_t* vnode)
{
    if (vnode->data != NULL)
    {
        free(vnode->data);
        vnode->data = NULL;
        vnode->size = 0;
    }
}

static vnode_ops_t vnodeOps = {
    .create = tmpfs_create,
    .truncate = tmpfs_truncate,
    .link = tmpfs_link,
    .readlink = tmpfs_readlink,
    .symlink = tmpfs_symlink,
    .remove = tmpfs_remove,
    .cleanup = tmpfs_vnode_cleanup,
};

static dentry_ops_t dentryOps = {
    .iterate = dentry_generic_iterate,
};

static void tmpfs_superblock_cleanup(superblock_t* superblock)
{
    UNUSED(superblock);

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

    vnode_t* vnode = tmpfs_vnode_new(superblock, VREG, in->data, in->size);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create tmpfs file vnode");
    }
    UNREF_DEFER(vnode);

    dentry_make_positive(dentry, vnode);

    return REF(dentry);
}

static dentry_t* tmpfs_load_dir(superblock_t* superblock, dentry_t* parent, const char* name, const boot_dir_t* in)
{
    tmpfs_superblock_data_t* superData = superblock->data;

    dentry_t* dentry = dentry_new(superblock, parent, name);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create tmpfs dentry");
    }
    UNREF_DEFER(dentry);

    vnode_t* vnode = tmpfs_vnode_new(superblock, VDIR, NULL, 0);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create tmpfs vnode");
    }
    UNREF_DEFER(vnode);

    tmpfs_dentry_add(dentry);
    dentry_make_positive(dentry, vnode);

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

static dentry_t* tmpfs_mount(filesystem_t* fs, const char* options, void* data)
{
    UNUSED(data);

    if (options != NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    superblock_t* superblock = superblock_new(fs, &superOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(superblock);

    superblock->blockSize = 0;
    superblock->maxFileSize = UINT64_MAX;

    tmpfs_superblock_data_t* tmpfsData = malloc(sizeof(tmpfs_superblock_data_t));
    if (tmpfsData == NULL)
    {
        return NULL;
    }
    list_init(&tmpfsData->dentrys);
    lock_init(&tmpfsData->lock);
    superblock->data = tmpfsData;

    if (!initialized)
    {
        boot_info_t* bootInfo = boot_info_get();
        const boot_disk_t* disk = &bootInfo->disk;

        dentry_t* root = tmpfs_load_dir(superblock, NULL, NULL, disk->root);
        if (root == NULL)
        {
            return NULL;
        }

        superblock->root = root;
        return REF(superblock->root);
    }

    dentry_t* dentry = dentry_new(superblock, NULL, NULL);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    vnode_t* vnode = tmpfs_vnode_new(superblock, VDIR, NULL, 0);
    if (vnode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(vnode);

    tmpfs_dentry_add(dentry);
    dentry_make_positive(dentry, vnode);

    superblock->root = dentry;
    return REF(superblock->root);
}

static vnode_t* tmpfs_vnode_new(superblock_t* superblock, vtype_t type, void* buffer, uint64_t size)
{
    vnode_t* vnode = vnode_new(superblock, type, &vnodeOps, &fileOps);
    if (vnode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(vnode);

    if (buffer != NULL)
    {
        vnode->data = malloc(size);
        if (vnode->data == NULL)
        {
            return NULL;
        }
        memcpy(vnode->data, buffer, size);
        vnode->size = size;
    }
    else
    {
        vnode->data = NULL;
        vnode->size = 0;
    }

    return REF(vnode);
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

    process_t* process = process_current();
    assert(process != NULL);

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        panic(NULL, "Failed to get process namespace");
    }
    UNREF_DEFER(ns);

    mount_t* temp = namespace_mount(ns, NULL, &tmpfs, NULL, MODE_PROPAGATE | MODE_ALL_PERMS, NULL);
    if (temp == NULL)
    {
        panic(NULL, "Failed to mount tmpfs");
    }
    UNREF(temp);
    LOG_INFO("tmpfs initialized\n");

    initialized = true;
}
