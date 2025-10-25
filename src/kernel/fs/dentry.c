#include "dentry.h"

#include <stdio.h>

#include "log/log.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "sync/mutex.h"
#include "vfs.h"

static void dentry_free(dentry_t* dentry)
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
        dentry->inode = NULL;
    }

    if (!DENTRY_IS_ROOT(dentry))
    {
        if (!(atomic_load(&dentry->flags) & DENTRY_NEGATIVE))
        {
            mutex_acquire(&dentry->parent->childrenMutex);
            list_remove(&dentry->parent->children, &dentry->siblingEntry);
            mutex_release(&dentry->parent->childrenMutex);
        }

        DEREF(dentry->parent);
        dentry->parent = NULL;
    }

    heap_free(dentry);
}

dentry_t* dentry_new(superblock_t* superblock, dentry_t* parent, const char* name)
{
    if (name == NULL || superblock == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    size_t nameLen = strnlen_s(name, MAX_NAME);
    if (nameLen >= MAX_NAME || nameLen == 0)
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
    dentry->parent = parent != NULL
        ? REF(parent)
        : dentry; // We set its parent now but its only added to its list when it is made positive.
    list_entry_init(&dentry->siblingEntry);
    list_init(&dentry->children);
    mutex_init(&dentry->childrenMutex);
    dentry->superblock = REF(superblock);
    dentry->ops = dentry->superblock != NULL ? dentry->superblock->dentryOps : NULL;
    dentry->private = NULL;
    atomic_init(&dentry->flags, DENTRY_NEGATIVE);
    atomic_init(&dentry->mountCount, 0);

    return dentry;
}

uint64_t dentry_make_positive(dentry_t* dentry, inode_t* inode)
{
    if (dentry == NULL || inode == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&dentry->childrenMutex);

    // Sanity checks.
    assert(atomic_load(&dentry->flags) & DENTRY_NEGATIVE);
    assert(dentry->inode == NULL);

    if (vfs_add_dentry(dentry) == ERR)
    {
        return ERR;
    }

    dentry->inode = REF(inode);

    if (!DENTRY_IS_ROOT(dentry))
    {
        MUTEX_SCOPE(&dentry->parent->childrenMutex);
        list_push(&dentry->parent->children, &dentry->siblingEntry);
    }

    atomic_fetch_and(&dentry->flags, ~DENTRY_NEGATIVE);

    return 0;
}

void dentry_inc_mount_count(dentry_t* dentry)
{
    atomic_fetch_add(&dentry->mountCount, 1);
}

void dentry_dec_mount_count(dentry_t* dentry)
{
    atomic_fetch_sub(&dentry->mountCount, 1);
}

typedef struct
{
    uint64_t index;
    uint64_t total;
    dirent_t* buffer;
    uint64_t count;
    uint64_t* offset;
    uint32_t flags;
    char basePath[MAX_PATH];
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
        strncpy(dirent->name, name, MAX_PATH - 1);
        dirent->name[MAX_PATH - 1] = '\0';
    }
    ctx->index++;
    ctx->total++;
}

static void getdents_recursive_traversal(getdents_ctx_t* ctx, dentry_t* dentry)
{
    if (strcmp(ctx->basePath, "") != 0)
    {
        char fullPath[MAX_PATH];
        snprintf(fullPath, MAX_PATH, "%s/%s", ctx->basePath, dentry->name);
        getdents_write(ctx, dentry->inode->number, dentry->inode->type, fullPath);
    }

    MUTEX_SCOPE(&dentry->childrenMutex);

    dentry_t* child;
    LIST_FOR_EACH(child, &dentry->children, siblingEntry)
    {
        if (ctx->flags & PATH_RECURSIVE && child->inode->type == INODE_DIR)
        {
            char originalBasePath[MAX_PATH];
            strncpy(originalBasePath, ctx->basePath, MAX_PATH - 1);
            originalBasePath[MAX_PATH - 1] = '\0';

            snprintf(ctx->basePath, MAX_PATH, "%s/%s", originalBasePath, child->name);
            getdents_recursive_traversal(ctx, child);

            strncpy(ctx->basePath, originalBasePath, MAX_PATH - 1);
            ctx->basePath[MAX_PATH - 1] = '\0';
        }
        else
        {
            char fullPath[MAX_PATH];
            snprintf(fullPath, MAX_PATH, "%s/%s", ctx->basePath, child->name);
            getdents_write(ctx, child->inode->number, child->inode->type, fullPath);
        }
    }
}

uint64_t dentry_generic_getdents(dentry_t* dentry, dirent_t* buffer, uint64_t count, uint64_t* offset,
    path_flags_t flags)
{
    getdents_ctx_t ctx = {
        .index = 0,
        .total = 0,
        .buffer = buffer,
        .count = count,
        .offset = offset,
        .flags = flags,
        .basePath = "",
    };

    MUTEX_SCOPE(&dentry->childrenMutex);

    getdents_write(&ctx, dentry->inode->number, dentry->inode->type, ".");
    getdents_write(&ctx, dentry->parent->inode->number, dentry->parent->inode->type, "..");

    if (flags & PATH_RECURSIVE)
    {
        dentry_t* child;
        LIST_FOR_EACH(child, &dentry->children, siblingEntry)
        {
            if (child->inode->type == INODE_DIR)
            {
                snprintf(ctx.basePath, MAX_PATH, "%s", child->name);
                getdents_recursive_traversal(&ctx, child);
                ctx.basePath[0] = '\0';
            }
            else
            {
                getdents_write(&ctx, child->inode->number, child->inode->type, child->name);
            }
        }
    }
    else
    {
        dentry_t* child;
        LIST_FOR_EACH(child, &dentry->children, siblingEntry)
        {
            getdents_write(&ctx, child->inode->number, child->inode->type, child->name);
        }
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
