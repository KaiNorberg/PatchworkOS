#include <kernel/fs/sysfs.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>

static mount_t* devMount = NULL;

static file_ops_t dirOps = {
    .seek = file_generic_seek,
};

static dentry_ops_t dentryOps = {
    .iterate = dentry_generic_iterate,
};

typedef struct
{
    const superblock_ops_t* superblockOps;
    const inode_ops_t* inodeOps;
    void* private;
} sysfs_mount_ctx_t;

static dentry_t* sysfs_mount(filesystem_t* fs, const char* devName, void* private)
{
    UNUSED(devName);
    UNUSED(private);

    sysfs_mount_ctx_t* ctx = private;

    superblock_t* superblock =
        superblock_new(fs, VFS_DEVICE_NAME_NONE, ctx != NULL ? ctx->superblockOps : NULL, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(superblock);

    inode_t* inode = inode_new(superblock, vfs_id_get(), INODE_DIR, ctx != NULL ? ctx->inodeOps : NULL, &dirOps);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);
    inode->private = ctx != NULL ? ctx->private : NULL;

    dentry_t* dentry = dentry_new(superblock, NULL, VFS_ROOT_ENTRY_NAME);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    dentry_make_positive(dentry, inode);

    superblock->root = dentry;
    return REF(superblock->root);
}

static filesystem_t sysfs = {
    .name = SYSFS_NAME,
    .mount = sysfs_mount,
};

void sysfs_init(void)
{
    LOG_INFO("registering sysfs\n");
    if (filesystem_register(&sysfs) == ERR)
    {
        panic(NULL, "Failed to register sysfs");
    }

    devMount = sysfs_mount_new("dev", NULL, MODE_PROPAGATE_CHILDREN | MODE_ALL_PERMS, NULL, NULL, NULL);
    if (devMount == NULL)
    {
        panic(NULL, "Failed to create /dev filesystem");
    }
    LOG_INFO("sysfs initialized\n");
}

dentry_t* sysfs_get_dev(void)
{
    return REF(devMount->source);
}

mount_t* sysfs_mount_new(const char* name, namespace_handle_t* ns, mode_t mode, const inode_ops_t* inodeOps,
    const superblock_ops_t* superblockOps, void* private)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (ns == NULL)
    {
        process_t* process = sched_process();
        ns = &process->ns;
    }

    path_t rootPath = PATH_EMPTY;
    namespace_get_root(ns, &rootPath);
    PATH_DEFER(&rootPath);

    dentry_t* dentry = dentry_lookup(&rootPath, name);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    path_t mountpoint = PATH_CREATE(rootPath.mount, dentry);
    PATH_DEFER(&mountpoint);

    sysfs_mount_ctx_t ctx = {
        .superblockOps = superblockOps,
        .inodeOps = inodeOps,
        .private = private,
    };

    return namespace_mount(ns, &mountpoint, SYSFS_NAME, VFS_DEVICE_NAME_NONE, mode, &ctx);
}

mount_t* sysfs_submount_new(const path_t* parent, const char* name, namespace_handle_t* ns, mode_t mode,
    const inode_ops_t* inodeOps, const superblock_ops_t* superblockOps, void* private)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent->dentry->superblock->fs != &sysfs)
    {
        errno = EXDEV;
        return NULL;
    }

    if (ns == NULL)
    {
        process_t* process = sched_process();
        ns = &process->ns;
    }

    inode_t* inode = inode_new(parent->dentry->superblock, vfs_id_get(), INODE_DIR, NULL, &dirOps);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);

    dentry_t* dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    dentry_make_positive(dentry, inode);

    sysfs_mount_ctx_t ctx = {
        .superblockOps = superblockOps,
        .inodeOps = inodeOps,
        .private = private,
    };

    path_t mountpoint = PATH_CREATE(parent->mount, dentry);
    PATH_DEFER(&mountpoint);

    return namespace_mount(ns, &mountpoint, SYSFS_NAME, VFS_DEVICE_NAME_NONE, mode, &ctx);
}

dentry_t* sysfs_dir_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* private)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent == NULL)
    {
        parent = devMount->source;
    }

    if (parent->superblock->fs != &sysfs)
    {
        errno = EXDEV;
        return NULL;
    }

    dentry_t* dir = dentry_new(parent->superblock, parent, name);
    if (dir == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dir);

    inode_t* inode = inode_new(parent->superblock, vfs_id_get(), INODE_DIR, inodeOps, &dirOps);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);
    inode->private = private;

    dentry_make_positive(dir, inode);

    return REF(dir);
}

dentry_t* sysfs_file_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, const file_ops_t* fileOps,
    void* private)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent == NULL)
    {
        parent = devMount->source;
    }

    if (parent->superblock->fs != &sysfs)
    {
        errno = EXDEV;
        return NULL;
    }

    dentry_t* dentry = dentry_new(parent->superblock, parent, name);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    inode_t* inode = inode_new(parent->superblock, vfs_id_get(), INODE_FILE, inodeOps, fileOps);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);
    inode->private = private;

    dentry_make_positive(dentry, inode);

    return REF(dentry);
}

dentry_t* sysfs_symlink_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* private)
{
    if (parent == NULL || name == NULL || inodeOps == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent->superblock->fs != &sysfs)
    {
        errno = EXDEV;
        return NULL;
    }

    dentry_t* dentry = dentry_new(parent->superblock, parent, name);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    inode_t* inode = inode_new(parent->superblock, vfs_id_get(), INODE_SYMLINK, inodeOps, NULL);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);
    inode->private = private;

    dentry_make_positive(dentry, inode);

    return REF(dentry);
}

uint64_t sysfs_files_create(dentry_t* parent, const sysfs_file_desc_t* descs, void* private, list_t* out)
{
    if (descs == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (parent == NULL)
    {
        parent = devMount->source;
    }

    if (parent->superblock->fs != &sysfs)
    {
        errno = EXDEV;
        return ERR;
    }

    list_t createdList = LIST_CREATE(createdList);

    for (const sysfs_file_desc_t* desc = descs; desc->name != NULL; desc++)
    {
        dentry_t* file = sysfs_file_new(parent, desc->name, desc->inodeOps, desc->fileOps, private);
        if (file == NULL)
        {
            while (!list_is_empty(&createdList))
            {
                UNREF(CONTAINER_OF_SAFE(list_pop_first(&createdList), dentry_t, otherEntry));
            }
            return ERR;
        }

        list_push_back(&createdList, &file->otherEntry);
    }

    uint64_t count = list_length(&createdList);

    if (out == NULL)
    {
        while (!list_is_empty(&createdList))
        {
            UNREF(CONTAINER_OF_SAFE(list_pop_first(&createdList), dentry_t, otherEntry));
        }
        return count;
    }

    while (!list_is_empty(&createdList))
    {
        dentry_t* file = CONTAINER_OF_SAFE(list_pop_first(&createdList), dentry_t, otherEntry);
        list_push_back(out, &file->otherEntry);
    }
    return count;
}