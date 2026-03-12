#include <kernel/fs/devfs.h>

#include <kernel/fs/binding.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
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

static vnode_class_t dirClass = {
    .name = "devfs dir",
    .type = FILE_TYPE_DIRECTORY,
    .handlers =
        {
            VNODE_DIR_HANDLERS(),
        },
};

static status_t devfs_clone_open(irp_t* irp)
{
    irp_frame_t* frame = irp_current(irp);
    file_t* file = frame->file;
    if (file == NULL)
    {
        return ERR(FS, EXPECT_FILE);
    }

    return file_redirect(file, root);
}

static vnode_class_t cloneClass = {
    .name = "devfs clone",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_OPEN] = devfs_clone_open,
        },
};

static filesystem_t devfs = {
    .name = DEVFS_NAME,
    .clone = &cloneClass,
};

void devfs_init(void)
{
    vnode_t* vnode = vnode_new(volume_new(), &dirClass, 0);
    if (vnode == NULL)
    {
        panic(NULL, "Failed to create devfs root vnode");
    }
    UNREF_DEFER(vnode);

    root = dentry_new(NULL, NULL);
    if (root == NULL)
    {
        panic(NULL, "Failed to create devfs root dentry");
    }

    dentry_make_positive(root, vnode);

    status_t status = filesystem_register(&devfs);
    if (IS_ERR(status))
    {
        panic(NULL, "Failed to register devfs %Y", status);
    }
}

dentry_t* devfs_dentry_new(dentry_t* parent, const char* name, const vnode_class_t* cls, void* data)
{
    if (name == NULL)
    {
        return NULL;
    }

    if (parent == NULL)
    {
        parent = root;
    }

    assert(DENTRY_IS_POSITIVE(parent));
    assert(parent->vnode->volume == root->vnode->volume);

    dentry_t* dentry = dentry_new(parent, name);
    if (dentry == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(dentry);

    vnode_t* vnode = vnode_new(parent->vnode->volume, cls, vnode_hash(parent->vnode->number, name));
    if (vnode == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(vnode);
    vnode->data = data;

    dentry_make_positive(dentry, vnode);

    return REF(dentry);
}

bool devfs_dentrys_new(list_t* out, dentry_t* parent, const devfs_desc_t* descs, size_t count)
{
    if (parent == NULL)
    {
        parent = root;
    }

    assert(parent->vnode->volume == root->vnode->volume);

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