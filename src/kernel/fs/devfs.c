#include <kernel/fs/devfs.h>

#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/binding.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/fs/volume.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>
#include <sys/list.h>

static dentry_t* root = NULL;

static status_t devfs_mount(filesystem_t* fs, dentry_t** out, const char* options, void* data)
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

static filesystem_t devfs = {
    .name = DEVFS_NAME,
    .mount = devfs_mount,
};

static vnode_class_t rootClass = {.name = "devfs root", .type = VTYPE_DIRECTORY, VNODE_DIR_HANDLERS()};

void devfs_init(void)
{
    status_t status = filesystem_register(&devfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to register devfs");
    }

    volume_t* volume = volume_new(&devfs, NULL);
    if (volume == NULL)
    {
        panic(NULL, "Failed to create devfs volume");
    }
    UNREF_DEFER(volume);

    vnode_t* vnode = vnode_new(volume, &rootClass);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create devfs root vnode");
    }
    UNREF_DEFER(vnode);

    dentry_t* dentry = dentry_new(volume, NULL, NULL);
    if (dentry == NULL)
    {
        panic(NULL, "Failed to create devfs root dentry");
    }

    dentry_make_positive(dentry, vnode);
    volume->root = dentry;
    root = dentry;
}

dentry_t* devfs_dentry_new(dentry_t* parent, const char* name, const vnode_class_t* cls, void* data)
{
    if (name == NULL || cls == NULL)
    {
        return NULL;
    }

    if (parent == NULL)
    {
        parent = root;
    }

    assert(parent->volume->fs == &devfs);

    dentry_t* dir = dentry_new(parent->volume, parent, name);
    if (dir == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dir);

    vnode_t* vnode = vnode_new(parent->volume, cls);
    if (vnode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(vnode);
    vnode->data = data;

    dentry_make_positive(dir, vnode);

    return REF(dir);
}

bool devfs_dentrys_new(list_t* out, dentry_t* parent, const devfs_desc_t* descs, size_t count)
{
    if (parent == NULL)
    {
        parent = root;
    }

    assert(parent->volume->fs == &devfs);

    list_t createdList = LIST_CREATE(createdList);

    for (size_t i = 0; i < count; i++)
    {
        const devfs_desc_t* desc = &descs[i];
        dentry_t* dentry = devfs_dentry_new(parent, desc->name, desc->cls, desc->data);
        if (dentry == NULL)
        {
            while (!list_is_empty(&createdList))
            {
                UNREF(CONTAINER_OF_SAFE(list_pop_front(&createdList), dentry_t, entry));
            }
            return false;
        }

        list_push_back(&createdList, &dentry->entry);
    }

    if (out == NULL)
    {
        while (!list_is_empty(&createdList))
        {
            UNREF(CONTAINER_OF_SAFE(list_pop_front(&createdList), dentry_t, entry));
        }
        return true;
    }

    while (!list_is_empty(&createdList))
    {
        dentry_t* file = CONTAINER_OF_SAFE(list_pop_front(&createdList), dentry_t, entry);
        list_push_back(out, &file->entry);
    }
    return true;
}

void devfs_dentrys_free(list_t* dentries)
{
    if (dentries == NULL)
    {
        return;
    }

    while (!list_is_empty(dentries))
    {
        UNREF(CONTAINER_OF_SAFE(list_pop_back(dentries), dentry_t, entry));
    }
}