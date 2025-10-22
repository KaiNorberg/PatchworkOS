#include "sysfs.h"

#include "fs/dentry.h"
#include "fs/namespace.h"
#include "log/log.h"
#include "log/panic.h"
#include "sync/lock.h"
#include "vfs.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
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

    systems_mount_ctx_t* ctx = (sysfs_mount_ctx_t*)private;
    if (ctx == NULL)
    {
        return NULL;
    }

    superblock_t* superblock = superblock_new(fs, VFS_DEVICE_NAME_NONE, ctx->superblockOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(superblock);

    inode_t* inode = inode_new(superblock, atomic_fetch_add(&newNum, 1), INODE_DIR, NULL, &dirOps);
    if (inode == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(inode);

    dentry_t* dentry = dentry_new(superblock, NULL, VFS_ROOT_ENTRY_NAME);
    if (dentry == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(dentry);

    if (dentry_make_positive(dentry, inode) == ERR)
    {
        return NULL;
    }

    return REF(superblock->root);
}

static filesystem_t sysfs = {
    .name = SYSFS_NAME,
    .mount = sysfs_mount,
};

void sysfs_init(void)
{
    LOG_INFO("registering sysfs\n");
    if (vfs_register_fs(&sysfs) == ERR)
    {
        panic(NULL, "Failed to register sysfs");
    }

    devMount = sysfs_superblock_new(NULL, "dev", NULL);
    if (devMount == NULL)
    {
        panic(NULL, "Failed to create /dev filesystem");
    }
    LOG_INFO("sysfs initialized\n");
}

dentry_t* sysfs_get_dev(void)
{
    return REF(devMount->superblock->root);
}

superblock_t* sysfs_superblock_new(const path_t* parent, const char* name, namespace_t* ns,
    const superblock_ops_t* superblockOps)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (ns == NULL)
    {
        process_t* kernelProcess = process_get_kernel();
        assert(kernelProcess != NULL);
        ns = &kernelProcess->namespace;
    }

    if (parent == NULL)
    {
        path_t rootPath = PATH_EMPTY;
        path_set(&rootPath, ns->rootMount, ns->rootMount->superblock->root);
        PATH_DEFER(&rootPath);

        dentry_t* dentry = vfs_get_or_lookup_dentry(&rootPath, name);
        if (dentry == NULL)
        {
            return NULL;
        }
        DEREF_DEFER(dentry);

        path_t mountpoint = PATH_EMPTY;
        path_set(&mountpoint, ns->rootMount, dentry);
        PATH_DEFER(&mountpoint);

        sysfs_mount_ctx_t ctx = {
            .superblockOps = superblockOps,
        };

        mount_t* mount = NULL;
        uint64_t result = namespace_mount(ns, &mountpoint, VFS_DEVICE_NAME_NONE, SYSFS_NAME, &mount, &ctx);
        if (result == ERR)
        {
            return NULL;
        }
        DEREF_DEFER(mount);

        return REF(mount->superblock);
    }

    if (parent->dentry->superblock->fs != &sysfs)
    {
        errno = EXDEV;
        return NULL;
    }

    dentry_t* dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
    if (dentry == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(dentry);

    path_t mountpoint = PATH_EMPTY;
    path_set(&mountpoint, parent->mount, dentry);
    PATH_DEFER(&mountpoint);

    mount_t* mount = NULL;
    uint64_t result = namespace_mount(ns, &mountpoint, VFS_DEVICE_NAME_NONE, SYSFS_NAME, &mount, NULL);
    if (result == ERR)
    {
        return NULL;
    }
    DEREF_DEFER(mount);

    return REF(mount->superblock);
}

dentry_t* sysfs_dir_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* private)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent == NULL)
    {
        parent = devMount->superblock->root;
    }

    dentry_t* dir = dentry_new(parent->superblock, parent, name);
    if (dir == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(dir);

    inode_t* inode = inode_new(parent->superblock, atomic_fetch_add(&newNum, 1), INODE_DIR, inodeOps, &dirOps);
    if (inode == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(inode);

    inode->private = private;

    if (dentry_make_positive(dir, inode) == ERR)
    {
        return NULL;
    }

    return REF(dir);
}

dentry_t* sysfs_file_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, const file_ops_t* fileOps,
    void* private)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent == NULL)
    {
        parent = devMount->superblock->root;
    }

    dentry_t* file = dentry_new(parent->superblock, parent, name);
    if (file == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(file);

    inode_t* inode = inode_new(parent->superblock, atomic_fetch_add(&newNum, 1), INODE_FILE, inodeOps, fileOps);
    if (inode == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(inode);

    inode->private = private;

    if (dentry_make_positive(file, inode) == ERR)
    {
        return NULL;
    }

    return REF(file);
}
