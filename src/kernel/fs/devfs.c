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

static dentry_t* devfs_mount(filesystem_t* fs, const char* options, void* data)
{
    UNUSED(fs);
    UNUSED(data);

    if (options != NULL)
    {
        errno = EINVAL;
        return NULL;
    }

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

    superblock_t* superblock = superblock_new(&devfs, NULL, &dentryOps);
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

dentry_t* devfs_dir_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* data)
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
    inode->data = data;

    dentry_make_positive(dir, inode);

    return REF(dir);
}

dentry_t* devfs_file_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, const file_ops_t* fileOps,
    void* data)
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

    inode_t* inode = inode_new(parent->superblock, vfs_id_get(), INODE_REGULAR, inodeOps, fileOps);
    if (inode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(inode);
    inode->data = data;

    dentry_make_positive(dentry, inode);

    return REF(dentry);
}

dentry_t* devfs_symlink_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* data)
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
    inode->data = data;

    dentry_make_positive(dentry, inode);

    return REF(dentry);
}

uint64_t devfs_files_new(list_t* out, dentry_t* parent, const devfs_file_desc_t* descs)
{
    if (out == NULL || descs == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (parent == NULL)
    {
        parent = root;
    }

    if (parent->superblock->fs != &devfs)
    {
        errno = EXDEV;
        return ERR;
    }

    list_t createdList = LIST_CREATE(createdList);

    uint64_t count = 0;
    for (const devfs_file_desc_t* desc = descs; desc->name != NULL; desc++)
    {
        dentry_t* file = devfs_file_new(parent, desc->name, desc->inodeOps, desc->fileOps, desc->data);
        if (file == NULL)
        {
            while (!list_is_empty(&createdList))
            {
                UNREF(CONTAINER_OF_SAFE(list_pop_front(&createdList), dentry_t, otherEntry));
            }
            return ERR;
        }

        list_push_back(&createdList, &file->otherEntry);
        count++;
    }

    if (out == NULL)
    {
        while (!list_is_empty(&createdList))
        {
            UNREF(CONTAINER_OF_SAFE(list_pop_front(&createdList), dentry_t, otherEntry));
        }
        return count;
    }

    while (!list_is_empty(&createdList))
    {
        dentry_t* file = CONTAINER_OF_SAFE(list_pop_front(&createdList), dentry_t, otherEntry);
        list_push_back(out, &file->otherEntry);
    }
    return count;
}

void devfs_files_free(list_t* files)
{
    if (files == NULL)
    {
        return;
    }

    while (!list_is_empty(files))
    {
        UNREF(CONTAINER_OF_SAFE(list_pop_back(files), dentry_t, otherEntry));
    }
}