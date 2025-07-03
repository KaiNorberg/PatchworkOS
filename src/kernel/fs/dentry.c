#include "dentry.h"

#include "mem/heap.h"
#include "sched/thread.h"
#include "vfs.h"

dentry_t* dentry_new(superblock_t* superblock, dentry_t* parent, const char* name)
{
    if (strnlen_s(name, MAX_NAME) >= MAX_NAME)
    {
        errno = EINVAL;
        return NULL;
    }

    assert(parent == NULL || superblock == parent->superblock);

    dentry_t* dentry = heap_alloc(sizeof(dentry_t), HEAP_NONE);
    if (dentry == NULL)
    {
        return NULL;
    }

    map_entry_init(&dentry->mapEntry);
    dentry->id = vfs_get_new_id();
    atomic_init(&dentry->ref, 1);
    strncpy(dentry->name, name, MAX_NAME - 1);
    dentry->name[MAX_NAME - 1] = '\0';
    dentry->inode = NULL;
    dentry->parent = parent != NULL ? dentry_ref(parent) : dentry;
    list_entry_init(&dentry->siblingEntry);
    list_init(&dentry->children);
    dentry->superblock = superblock_ref(superblock);
    dentry->ops = dentry->superblock != NULL ? dentry->superblock->dentryOps : NULL;
    dentry->private = NULL;
    dentry->flags = DENTRY_NEGATIVE | DENTRY_LOOKUP_PENDING;
    wait_queue_init(&dentry->lookupWaitQueue);

    if (dentry->parent != NULL && dentry->parent != dentry)
    {
        lock_acquire(&dentry->parent->lock);
        list_push(&dentry->parent->children, &dentry->siblingEntry);
        lock_release(&dentry->parent->lock);
    }

    return dentry;
}

void dentry_make_positive(dentry_t* dentry, inode_t* inode)
{
    LOCK_DEFER(&dentry->lock);
    
    // Sanity checks.
    assert(dentry->flags & DENTRY_NEGATIVE);
    assert(dentry->inode == NULL);
    
    if (inode != NULL) 
    {
        dentry->inode = inode_ref(inode);
        dentry->flags &= ~DENTRY_NEGATIVE;
    }    
    dentry->flags &= ~DENTRY_LOOKUP_PENDING;
    
    wait_unblock(&dentry->lookupWaitQueue, UINT64_MAX);
}

void dentry_free(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return;
    }

    vfs_remove_dentry(dentry);

    if (dentry->inode != NULL)
    {
        inode_deref(dentry->inode);
    }

    if (dentry->parent != NULL && dentry->parent != dentry)
    {
        lock_acquire(&dentry->parent->lock);
        list_remove(&dentry->siblingEntry);
        lock_release(&dentry->parent->lock);

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
        dentry_free(dentry);
    }
}