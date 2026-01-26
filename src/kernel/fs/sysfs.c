#include <kernel/fs/sysfs.h>

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

static status_t sysfs_mount(filesystem_t* fs, dentry_t** out, const char* options, void* data)
{
    UNUSED(fs);
    UNUSED(data);

    if (options != NULL)
    {
        return ERR(FS, INVAL);
    }

    *out = REF(root);
    return OK;
}

static filesystem_t sysfs = {
    .name = SYSFS_NAME,
    .mount = sysfs_mount,
};

void sysfs_init(void)
{
    status_t status = filesystem_register(&sysfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to register sysfs");
    }

    superblock_t* superblock = superblock_new(&sysfs, NULL, &dentryOps);
    if (superblock == NULL)
    {
        panic(NULL, "Failed to create sysfs superblock");
    }
    UNREF_DEFER(superblock);

    vnode_t* vnode = vnode_new(superblock, VDIR, NULL, NULL);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create sysfs root vnode");
    }
    UNREF_DEFER(vnode);

    dentry_t* dentry = dentry_new(superblock, NULL, NULL);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create sysfs root dentry");
    }

    dentry_make_positive(dentry, vnode);
    superblock->root = dentry;
    root = dentry;

    process_t* process = process_current();
    assert(process != NULL);

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        panic(NULL, "Failed to get process namespace");
    }
    UNREF_DEFER(ns);

    path_t target = cwd_get(&process->cwd, ns);
    PATH_DEFER(&target);

    pathname_t pathname;
    status = pathname_init(&pathname, "/sys");
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to init pathname for /sys");
    }

    status = path_walk(&target, &pathname, ns);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to walk to /sys");
    }

    status = namespace_mount(ns, &target, &sysfs, NULL, MODE_PROPAGATE | MODE_ALL_PERMS, NULL, NULL);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to mount sysfs");
    }
    LOG_INFO("sysfs mounted to '/sys'\n");
}

dentry_t* sysfs_dir_new(dentry_t* parent, const char* name, const vnode_ops_t* vnodeOps, void* data)
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

dentry_t* sysfs_file_new(dentry_t* parent, const char* name, const vnode_ops_t* vnodeOps, const file_ops_t* fileOps,
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

dentry_t* sysfs_symlink_new(dentry_t* parent, const char* name, const vnode_ops_t* vnodeOps, void* data)
{
    if (parent == NULL || name == NULL || vnodeOps == NULL)
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

bool sysfs_files_new(list_t* out, dentry_t* parent, const sysfs_file_desc_t* descs)
{
    if (out == NULL || descs == NULL)
    {
        return false;
    }

    if (parent == NULL)
    {
        parent = root;
    }

    if (parent->superblock->fs != &sysfs)
    {
        return false;
    }

    list_t createdList = LIST_CREATE(createdList);

    for (const sysfs_file_desc_t* desc = descs; desc->name != NULL; desc++)
    {
        dentry_t* file = sysfs_file_new(parent, desc->name, desc->vnodeOps, desc->fileOps, desc->data);
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

void sysfs_files_free(list_t* files)
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