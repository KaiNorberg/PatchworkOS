#include <kernel/fs/devfs.h>

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

static dentry_t* root = NULL;

static file_ops_t dirOps = {
    .seek = file_generic_seek,
};

static dentry_ops_t dentryOps = {
    .iterate = dentry_generic_iterate,
};

static dentry_t* devfs_mount(filesystem_t* fs, block_device_t* device, void* private)
{
    UNUSED(fs);
    UNUSED(device);
    UNUSED(private);

    return REF(root);
}

static filesystem_t devfs = {
    .name = DEVFS_NAME,
    .mount = devfs_mount,
};

void devfs_init(void)
{
    if (filesystem_register(&devfs) == ERR)
    {
        panic(NULL, "Failed to register devfs");
    }

    superblock_t* superblock = superblock_new(&devfs, NULL, NULL, &dentryOps);
    if (superblock == NULL)
    {
        panic(NULL, "Failed to create devfs superblock");
    }
    UNREF_DEFER(superblock);

    inode_t* inode = inode_new(superblock, vfs_id_get(), INODE_DIR, NULL, &dirOps);
    if (inode == NULL)
    {
        panic(NULL, "Failed to create devfs root inode");
    }
    UNREF_DEFER(inode);

    dentry_t* dentry = dentry_new(superblock, NULL, NULL);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create devfs root dentry");
    }

    dentry_make_positive(dentry, inode);
    superblock->root = dentry;
    root = dentry;
}

dentry_t* devfs_dir_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* private)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent == NULL)
    {
        parent = root;
    }

    if (parent->superblock->fs != &devfs)
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

dentry_t* devfs_file_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, const file_ops_t* fileOps,
    void* private)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent == NULL)
    {
        parent = root;
    }

    if (parent->superblock->fs != &devfs)
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

dentry_t* devfs_symlink_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* private)
{
    if (parent == NULL || name == NULL || inodeOps == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent->superblock->fs != &devfs)
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