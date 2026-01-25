#include <kernel/fs/devfs.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/list.h>

static dentry_t* root = NULL;

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
    if (filesystem_register(&devfs) == _FAIL)
    {
        panic(NULL, "Failed to register devfs");
    }

    superblock_t* superblock = superblock_new(&devfs, NULL, &dentryOps);
    if (superblock == NULL)
    {
        panic(NULL, "Failed to create devfs superblock");
    }
    UNREF_DEFER(superblock);

    vnode_t* vnode = vnode_new(superblock, VDIR, NULL, NULL);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create devfs root vnode");
    }
    UNREF_DEFER(vnode);

    dentry_t* dentry = dentry_new(superblock, NULL, NULL);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create devfs root dentry");
    }

    dentry_make_positive(dentry, vnode);
    superblock->root = dentry;
    root = dentry;
}

dentry_t* devfs_dir_new(dentry_t* parent, const char* name, const vnode_ops_t* vnodeOps, void* data)
{
    if (name == NULL)
    {
        return NULL;
    }

    if (parent == NULL)
    {
        parent = root;
    }

    assert(parent->superblock->fs != &devfs);

    dentry_t* dir = dentry_new(parent->superblock, parent, name);
    if (dir == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dir);

    vnode_t* vnode = vnode_new(parent->superblock, VDIR, vnodeOps, NULL);
    if (vnode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(vnode);
    vnode->data = data;

    dentry_make_positive(dir, vnode);

    return REF(dir);
}

dentry_t* devfs_file_new(dentry_t* parent, const char* name, const vnode_ops_t* vnodeOps, const file_ops_t* fileOps,
    void* data)
{
    if (parent == NULL)
    {
        parent = root;
    }

    assert(parent->superblock->fs != &devfs);

    dentry_t* dentry = dentry_new(parent->superblock, parent, name);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    vnode_t* vnode = vnode_new(parent->superblock, VREG, vnodeOps, fileOps);
    if (vnode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(vnode);
    vnode->data = data;

    dentry_make_positive(dentry, vnode);

    return REF(dentry);
}

dentry_t* devfs_symlink_new(dentry_t* parent, const char* name, const vnode_ops_t* vnodeOps, void* data)
{
    if (parent == NULL || name == NULL || vnodeOps == NULL)
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

    vnode_t* vnode = vnode_new(parent->superblock, VSYMLINK, vnodeOps, NULL);
    if (vnode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(vnode);
    vnode->data = data;

    dentry_make_positive(dentry, vnode);

    return REF(dentry);
}

bool devfs_files_new(list_t* out, dentry_t* parent, const devfs_file_desc_t* descs)
{
    if (parent == NULL)
    {
        parent = root;
    }

    assert(parent->superblock->fs != &devfs);

    list_t createdList = LIST_CREATE(createdList);

    for (const devfs_file_desc_t* desc = descs; desc->name != NULL; desc++)
    {
        dentry_t* file = devfs_file_new(parent, desc->name, desc->vnodeOps, desc->fileOps, desc->data);
        if (file == NULL)
        {
            while (!list_is_empty(&createdList))
            {
                UNREF(CONTAINER_OF_SAFE(list_pop_front(&createdList), dentry_t, otherEntry));
            }
            return false;
        }

        list_push_back(&createdList, &file->otherEntry);
    }

    if (out == NULL)
    {
        while (!list_is_empty(&createdList))
        {
            UNREF(CONTAINER_OF_SAFE(list_pop_front(&createdList), dentry_t, otherEntry));
        }
        return true;
    }

    while (!list_is_empty(&createdList))
    {
        dentry_t* file = CONTAINER_OF_SAFE(list_pop_front(&createdList), dentry_t, otherEntry);
        list_push_back(out, &file->otherEntry);
    }
    return true;
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