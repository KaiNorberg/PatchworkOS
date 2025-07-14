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

        if (dentry->parent != NULL && dentry->parent != dentry)
        {
            LOCK_SCOPE(&dentry->parent->lock);
            list_push(&dentry->parent->children, &dentry->siblingEntry);
        }
    }
    dentry->flags &= ~DENTRY_LOOKUP_PENDING;

    wait_unblock(&dentry->lookupWaitQueue, WAIT_ALL);
}

typedef struct
{
    uint64_t index;
    uint64_t total;
    dirent_t* buffer;
    uint64_t count;
    uint64_t* offset;
} getdents_ctx_t;

static void getdents_write(getdents_ctx_t* ctx, inode_number_t number, inode_type_t type, const char* name)
{
    uint64_t start = *ctx->offset / sizeof(dirent_t);
    uint64_t amount = ctx->count / sizeof(dirent_t);

    if (ctx->index >= start && ctx->index < start + amount)
    {
        dirent_t* dirent = &ctx->buffer[ctx->index - start];
        dirent->number = number;
        dirent->type = type;
        strncpy(dirent->name, name, MAX_NAME - 1);
        dirent->name[MAX_NAME - 1] = '\0';
    }
    ctx->index++;
    ctx->total++;
}

uint64_t dentry_generic_getdents(dentry_t* dentry, dirent_t* buffer, uint64_t count, uint64_t* offset)
{
    getdents_ctx_t ctx = {
        .index = 0,
        .total = 0,
        .buffer = buffer,
        .count = count,
        .offset = offset
    };

    LOCK_SCOPE(&dentry->lock);

    MUTEX_SCOPE(&dentry->inode->mutex);

    getdents_write(&ctx, dentry->inode->number, dentry->inode->type, ".");
    getdents_write(&ctx, dentry->parent->inode->number, dentry->parent->inode->type, "..");

    dentry_t* child;
    LIST_FOR_EACH(child, &dentry->children, siblingEntry)
    {
        LOCK_SCOPE(&child->lock);
        getdents_write(&ctx, child->inode->number, child->inode->type, child->name);
    }

    dentry->inode->size = sizeof(dirent_t) * ctx.total;

    uint64_t start = *offset / sizeof(dirent_t);
    uint64_t max = count / sizeof(dirent_t);

    if (start >= ctx.total)
    {
        return 0;
    }

    uint64_t entriesWritten = MIN(ctx.total - start, max);
    uint64_t bytesWritten = entriesWritten * sizeof(dirent_t);

    *offset += bytesWritten;
    return bytesWritten;
}
