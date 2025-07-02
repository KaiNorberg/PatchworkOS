#include "dentry.h"

#include "mem/heap.h"
#include "sched/thread.h"
#include "vfs.h"

dentry_t* dentry_new(superblock_t* superblock, const char* name, inode_t* inode)
{
    if (strnlen_s(name, MAX_NAME) >= MAX_NAME)
    {
        errno = EINVAL;
        return NULL;
    }

    dentry_t* dentry = heap_alloc(sizeof(dentry_t), HEAP_NONE);
    if (dentry == NULL)
    {
        return NULL;
    }

    map_entry_init(&dentry->mapEntry);
    dentry->id = vfs_get_new_id();
    atomic_init(&dentry->ref, 1);
    strcpy(dentry->name, name);
    dentry->inode = inode_ref(inode);
    dentry->parent = NULL;
    dentry->superblock = superblock_ref(superblock);
    dentry->ops = dentry->superblock != NULL ? dentry->superblock->dentryOps : NULL;
    dentry->private = NULL;
    dentry->flags = DENTRY_NONE;
    lock_init(&dentry->lock);

    return dentry;
}

void dentry_free(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return;
    }

    if (dentry->inode != NULL)
    {
        inode_deref(dentry->inode);
    }

    if (dentry->parent != NULL)
    {
        dentry_deref(dentry->parent);
    }

    if (dentry->ops != NULL && dentry->ops->cleanup != NULL)
    {
        dentry->ops->cleanup(dentry);
    }

    heap_free(dentry);
}

dentry_t* dentry_ref(dentry_t* dentry)
{
    if (dentry != NULL)
    {
        atomic_fetch_add(&dentry->ref, 1);
    }
    return dentry;
}

void dentry_deref(dentry_t* dentry)
{
    if (dentry != NULL && atomic_fetch_sub(&dentry->ref, 1) <= 1)
    {
        vfs_remove_dentry(dentry);
        dentry_free(dentry);
    }
}