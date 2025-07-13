#include "dentry.h"

#include "log/log.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "sync/lock.h"
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

    ref_init(&dentry->ref, dentry_free);
    map_entry_init(&dentry->mapEntry);
    dentry->id = vfs_get_new_id();
    strncpy(dentry->name, name, MAX_NAME - 1);
    dentry->name[MAX_NAME - 1] = '\0';
    dentry->inode = NULL;
    dentry->parent = parent != NULL ? REF(parent) : dentry;
    list_entry_init(&dentry->siblingEntry);
    list_init(&dentry->children);
    dentry->superblock = REF(superblock);
    dentry->ops = dentry->superblock != NULL ? dentry->superblock->dentryOps : NULL;
    dentry->private = NULL;
    dentry->flags = DENTRY_NEGATIVE | DENTRY_LOOKUP_PENDING;
    lock_init(&dentry->lock);
    wait_queue_init(&dentry->lookupWaitQueue);

    return dentry;
}

void dentry_free(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return;
    }

    assert(dentry->parent != NULL);

    vfs_remove_dentry(dentry);

    if (dentry->ops != NULL && dentry->ops->cleanup != NULL)
    {
        dentry->ops->cleanup(dentry);
    }

    if (dentry->inode != NULL)
    {
        DEREF(dentry->inode);
    }

    if (dentry->parent != dentry)
    {
        lock_acquire(&dentry->parent->lock);
        list_remove(&dentry->siblingEntry);
        lock_release(&dentry->parent->lock);

        DEREF(dentry->parent);
        dentry->parent = NULL;
    }

    heap_free(dentry);
}

void dentry_make_positive(dentry_t* dentry, inode_t* inode)
{
    LOCK_SCOPE(&dentry->lock);

    // Sanity checks.
    assert(dentry->flags & DENTRY_NEGATIVE);
    assert(dentry->inode == NULL);

    if (inode != NULL)
    {
        dentry->inode = REF(inode);
        dentry->flags &= ~DENTRY_NEGATIVE;
    }
    dentry->flags &= ~DENTRY_LOOKUP_PENDING;

    if (dentry->parent != NULL && dentry->parent != dentry)
    {
        LOCK_SCOPE(&dentry->parent->lock);
        list_push(&dentry->parent->children, &dentry->siblingEntry);
    }

    wait_unblock(&dentry->lookupWaitQueue, UINT64_MAX);
}

uint64_t dentry_generic_getdirent(dentry_t* dentry, dirent_t* buffer, uint64_t amount)
{
    getdirent_ctx_t ctx = {0};

    getdirent_write(&ctx, buffer, amount, dentry->inode->number, dentry->inode->type, ".");
    getdirent_write(&ctx, buffer, amount, dentry->parent->inode->number, dentry->parent->inode->type, "..");

    LOCK_SCOPE(&dentry->lock);

    dentry_t* child;
    LIST_FOR_EACH(child, &dentry->children, siblingEntry)
    {
        LOCK_SCOPE(&child->lock);
        getdirent_write(&ctx, buffer, amount, child->inode->number, child->inode->type, child->name);
    }

    return ctx.total;
}
