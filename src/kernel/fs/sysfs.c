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

static dentry_t* defaultDir = NULL;

static file_ops_t dirOps = {
    .seek = file_generic_seek,
};

static dentry_ops_t dentryOps = {
    .getdents = dentry_generic_getdents,
};

static superblock_ops_t superOps = {

};

static dentry_t* sysfs_mount(filesystem_t* fs, const char* devName, void* private)
{
    (void)devName; // Unused

    sysfs_group_t* group = private;
    if (group == NULL)
    {
        LOG_ERR("sysfs_mount called with null group\n");
        errno = EINVAL;
        return NULL;
    }

    superblock_t* superblock = superblock_new(fs, VFS_DEVICE_NAME_NONE, &superOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(superblock);

    superblock->blockSize = 0;
    superblock->maxFileSize = UINT64_MAX;

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

    dentry_make_positive(dentry, inode);

    superblock->root = REF(dentry);
    group->root.dentry = REF(dentry);
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
    if (sysfs_group_init(&defaultGroup, NULL, "dev", NULL) == ERR)
    {
        panic(NULL, "Failed to initialize default sysfs group");
    }
    LOG_INFO("sysfs initialized\n");
}

sysfs_dir_t* sysfs_get_default(void)
{
    return &defaultGroup.root;
}

uint64_t sysfs_group_init(sysfs_group_t* group, sysfs_dir_t* parent, const char* name, namespace_t* ns)
{
    if (group == NULL || name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    path_t mountpoint = PATH_EMPTY;
    if (parent != NULL)
    {
        dentry_t* dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
        if (dentry == NULL)
        {
            LOG_ERR("failed to create dentry for sysfs group '%s'\n", name);
            return ERR;
        }
        DEREF_DEFER(dentry);

        inode_t* inode = inode_new(parent->dentry->superblock, atomic_fetch_add(&newNum, 1), INODE_DIR, NULL, &dirOps);
        if (inode == NULL)
        {
            LOG_ERR("failed to create inode for sysfs group '%s'\n", name);
            return ERR;
        }
        DEREF_DEFER(inode);

        if (vfs_add_dentry(dentry) == ERR)
        {
            LOG_ERR("failed to add dentry for sysfs group '%s'\n", name);
            return ERR;
        }
        dentry_make_positive(dentry, inode);

        path_set(&mountpoint, parent->group->mount, dentry);
    }
    else
    {
        path_t root = PATH_EMPTY;
        if (namespace_get_root_path(ns, &root) == ERR)
        {
            return ERR;
        }

        dentry_t* dentry = vfs_get_dentry(root.dentry, name);
        if (dentry == NULL)
        {
            path_put(&root);
            LOG_ERR("failed to get dentry for sysfs group '%s' in namespace root\n", name);
            return ERR;
        }

        path_set(&mountpoint, root.mount, dentry);

        path_put(&root);
        DEREF(dentry);
    }
    PATH_DEFER(&mountpoint);

    path_t mountedRoot = PATH_EMPTY;
    if (namespace_mount(ns, &mountpoint, VFS_DEVICE_NAME_NONE, SYSFS_NAME, &mountedRoot, group) == ERR)
    {
        LOG_ERR("failed to mount sysfs group '%s'\n", name);
        return ERR;
    }
    PATH_DEFER(&mountedRoot);

    group->root.dentry = REF(mountedRoot.dentry);
    group->root.group = group;
    group->mount = REF(mountedRoot.mount);
    return 0;
}

uint64_t sysfs_group_deinit(sysfs_group_t* group)
{
    if (group == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    DEREF(group->root.dentry);
    group->root.dentry = NULL;
    group->root.group = NULL;
    DEREF(group->mount);
    group->mount = NULL;
    return 0;
}

uint64_t sysfs_dir_init(sysfs_dir_t* dir, sysfs_dir_t* parent, const char* name, const inode_ops_t* inodeOps,
    void* private)
{
    if (dir == NULL || parent == NULL || name == NULL)
    {
        LOG_ERR("dir, parent or name is null\n");
        errno = EINVAL;
        return ERR;
    }

    dentry_t* dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
    if (dentry == NULL)
    {
        LOG_ERR("failed to create dentry for sysfs dir '%s'\n", name);
        return ERR;
    }
    DEREF_DEFER(dentry);
    dentry->private = dir;

    inode_t* inode = inode_new(parent->dentry->superblock, atomic_fetch_add(&newNum, 1), INODE_DIR, inodeOps, &dirOps);
    if (inode == NULL)
    {
        LOG_ERR("failed to create inode for sysfs dir '%s'\n", name);
        return ERR;
    }
    DEREF_DEFER(inode);
    inode->private = private;

    if (vfs_add_dentry(dentry) == ERR)
    {
        LOG_ERR("failed to add dentry for sysfs dir '%s'\n", name);
        return ERR;
    }
    dentry_make_positive(dentry, inode);

    dir->dentry = REF(dentry);
    dir->group = parent->group;
    return 0;
}

void sysfs_dir_deinit(sysfs_dir_t* dir)
{
    if (dir == NULL)
    {
        return;
    }

    DEREF(dir->dentry);
    dir->dentry = NULL;
    dir->group = NULL;
}

uint64_t sysfs_file_init(sysfs_file_t* file, sysfs_dir_t* parent, const char* name, const inode_ops_t* inodeOps,
    const file_ops_t* fileOps, void* private)
{
    if (file == NULL || parent == NULL || name == NULL)
    {
        LOG_ERR("file, parent or name is null\n");
        errno = EINVAL;
        return ERR;
    }

    dentry_t* dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
    if (dentry == NULL)
    {
        LOG_ERR("failed to create dentry for sysfs file '%s'\n", name);
        return ERR;
    }
    DEREF_DEFER(dentry);
    dentry->private = file;

    inode_t* inode = inode_new(parent->dentry->superblock, atomic_fetch_add(&newNum, 1), INODE_FILE, inodeOps, fileOps);
    if (inode == NULL)
    {
        LOG_ERR("failed to create inode for sysfs file '%s'\n", name);
        return ERR;
    }
    DEREF_DEFER(inode);
    inode->private = private;

    if (vfs_add_dentry(dentry) == ERR)
    {
        LOG_ERR("failed to add dentry for sysfs file '%s'\n", name);
        return ERR;
    }
    dentry_make_positive(dentry, inode);

    file->dentry = REF(dentry);
    return 0;
}

void sysfs_file_deinit(sysfs_file_t* file)
{
    if (file == NULL)
    {
        return;
    }

    DEREF(file->dentry);
    file->dentry = NULL;
}
