#include "sysfs.h"

#include "fs/dentry.h"
#include "log/log.h"
#include "log/panic.h"
#include "sync/lock.h"
#include "vfs.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <sys/list.h>

static _Atomic(inode_number_t) newNumber = ATOMIC_VAR_INIT(0);

static sysfs_group_t defaultGroup;

static file_ops_t dirOps = {
    .seek = file_generic_seek,
};

static dentry_ops_t dentryOps = {
    .getdents = dentry_generic_getdents,
};

static superblock_ops_t superOps = {

};

static dentry_t* sysfs_mount(filesystem_t* fs, superblock_flags_t flags, const char* devName, void* private)
{
    sysfs_group_t* group = private;

    superblock_t* superblock = superblock_new(fs, VFS_DEVICE_NAME_NONE, &superOps, &dentryOps);
    if (superblock == NULL)
    {
        return NULL;
    }
    REF_DEFER(superblock);

    superblock->blockSize = 0;
    superblock->maxFileSize = UINT64_MAX;
    superblock->flags = flags;

    inode_t* inode = inode_new(superblock, atomic_fetch_add(&newNumber, 1), INODE_DIR, NULL, &dirOps);
    if (inode == NULL)
    {
        return NULL;
    }
    REF_DEFER(inode);

    dentry_t* dentry = dentry_new(superblock, NULL, VFS_ROOT_ENTRY_NAME);
    if (dentry == NULL)
    {
        return NULL;
    }
    REF_DEFER(dentry);

    dentry->private = &group->root;
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
    if (sysfs_group_init(&defaultGroup, PATHNAME("/dev")) == ERR)
    {
        panic(NULL, "Failed to initialize default sysfs group");
    }
    LOG_INFO("sysfs initialized\n");
}

sysfs_dir_t* sysfs_get_default(void)
{
    return &defaultGroup.root;
}

uint64_t sysfs_group_init(sysfs_group_t* group, const pathname_t* mountpoint)
{
    if (group == NULL || mountpoint == NULL || !mountpoint->isValid)
    {
        errno = EINVAL;
        return ERR;
    }

    group->mountpoint = *mountpoint;

    // group->root.dentry is set in the mount function.
    if (vfs_mount(VFS_DEVICE_NAME_NONE, &group->mountpoint, SYSFS_NAME, SUPER_NONE, group) == ERR)
    {
        return ERR;
    }

    assert(group->root.dentry != NULL);
    return 0;
}

uint64_t sysfs_group_deinit(sysfs_group_t* group)
{
    if (group == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    return vfs_unmount(&group->mountpoint);
}

uint64_t sysfs_dir_init(sysfs_dir_t* dir, sysfs_dir_t* parent, const char* name, const inode_ops_t* inodeOps,
    void* private)
{
    if (dir == NULL || parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    dentry_t* dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
    if (dentry == NULL)
    {
        return ERR;
    }
    REF_DEFER(dentry);
    dentry->private = dir;

    inode_t* inode = inode_new(parent->dentry->superblock, atomic_fetch_add(&newNumber, 1), INODE_DIR, inodeOps, &dirOps);
    if (inode == NULL)
    {
        return ERR;
    }
    REF_DEFER(inode);
    inode->private = private;

    if (vfs_add_dentry(dentry) == ERR)
    {
        return ERR;
    }
    dentry_make_positive(dentry, inode);

    dir->dentry = REF(dentry);
    return 0;
}

void sysfs_dir_deinit(sysfs_dir_t* dir)
{
    if (dir == NULL || dir->dentry == NULL)
    {
        return;
    }

    DEREF(dir->dentry);
    dir->dentry = NULL;
}

uint64_t sysfs_file_init(sysfs_file_t* file, sysfs_dir_t* parent, const char* name, const inode_ops_t* inodeOps,
    const file_ops_t* fileOps, void* private)
{
    if (file == NULL || parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    dentry_t* dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
    if (dentry == NULL)
    {
        return ERR;
    }
    REF_DEFER(dentry);
    dentry->private = file;

    inode_t* inode =
        inode_new(parent->dentry->superblock, atomic_fetch_add(&newNumber, 1), INODE_FILE, inodeOps, fileOps);
    if (inode == NULL)
    {
        return ERR;
    }
    REF_DEFER(inode);
    inode->private = private;

    if (vfs_add_dentry(dentry) == ERR)
    {
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
