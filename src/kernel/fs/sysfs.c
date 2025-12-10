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
#include <sys/io.h>
#include <sys/list.h>

static _Atomic(inode_number_t) newNum = ATOMIC_VAR_INIT(0);

static mount_t* devMount = NULL;

static file_ops_t dirOps = {
    .seek = file_generic_seek,
};

static dentry_ops_t dentryOps = {
    .getdents = dentry_generic_getdents,
};

typedef struct
{
    const superblock_ops_t* superblockOps;
} sysfs_mount_ctx_t;

static dentry_t* sysfs_mount(filesystem_t* fs, const char* devName, void* private)
{
    (void)devName; // Unused
    (void)private; // Unused

    superblock_ops_t* superblockOps = NULL;
    if (private != NULL)
    {
        sysfs_mount_ctx_t* ctx = (sysfs_mount_ctx_t*)private;
        superblockOps = (superblock_ops_t*)ctx->superblockOps;
    }

    superblock_t* superblock = superblock_new(fs, VFS_DEVICE_NAME_NONE, superblockOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(superblock);

    inode_t* inode = inode_new(superblock, atomic_fetch_add(&newNum, 1), INODE_DIR, NULL, &dirOps);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);

    dentry_t* dentry = dentry_new(superblock, NULL, VFS_ROOT_ENTRY_NAME);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    dentry_make_positive(dentry, inode);

    superblock->root = REF(dentry);
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

    devMount =
        sysfs_mount_new(NULL, "dev", NULL, MOUNT_PROPAGATE_CHILDREN | MOUNT_PROPAGATE_PARENT, MODE_ALL_PERMS, NULL);
    if (devMount == NULL)
    {
        panic(NULL, "Failed to create /dev filesystem");
    }
    LOG_INFO("sysfs initialized\n");
}

dentry_t* sysfs_get_dev(void)
{
    return REF(devMount->root);
}

mount_t* sysfs_mount_new(const path_t* parent, const char* name, namespace_t* ns, mount_flags_t flags, mode_t mode,
    const superblock_ops_t* superblockOps)
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

    if (parent == NULL)
    {
        path_t rootPath = PATH_EMPTY;
        if (namespace_get_root_path(ns, &rootPath) == ERR)
        {
            return NULL;
        }
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
        };

        return namespace_mount(ns, &mountpoint, VFS_DEVICE_NAME_NONE, SYSFS_NAME, flags, mode, &ctx);
    }

    if (parent->dentry->superblock->fs != &sysfs)
    {
        errno = EXDEV;
        return NULL;
    }

    inode_t* inode = inode_new(parent->dentry->superblock, atomic_fetch_add(&newNum, 1), INODE_DIR, NULL, &dirOps);
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
    };

    path_t mountpoint = PATH_CREATE(parent->mount, dentry);
    mount_t* mount = namespace_mount(ns, &mountpoint, VFS_DEVICE_NAME_NONE, SYSFS_NAME, flags, mode, &ctx);
    path_put(&mountpoint);
    return mount;
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
        parent = devMount->root;
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

    inode_t* inode = inode_new(parent->superblock, atomic_fetch_add(&newNum, 1), INODE_DIR, inodeOps, &dirOps);
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
        parent = devMount->root;
    }

    if (parent->superblock->fs != &sysfs)
    {
        errno = EXDEV;
        return NULL;
    }

    dentry_t* file = dentry_new(parent->superblock, parent, name);
    if (file == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(file);

    inode_t* inode = inode_new(parent->superblock, atomic_fetch_add(&newNum, 1), INODE_FILE, inodeOps, fileOps);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);
    inode->private = private;

    dentry_make_positive(file, inode);

    return REF(file);
}
