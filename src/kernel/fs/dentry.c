#include <kernel/fs/dentry.h>

#include <kernel/sync/rcu.h>
#include <kernel/sync/seqlock.h>
#include <stdio.h>

#include <kernel/fs/inode.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/seqlock.h>

#include <stdlib.h>
#include <sys/list.h>

#define DENTRY_CACHE_SIZE 4096

static dentry_t* cache[DENTRY_CACHE_SIZE] = {NULL};
static seqlock_t lock = SEQLOCK_CREATE();

static uint64_t dentry_hash(dentry_id_t parentId, const char* name, size_t length)
{
    uint64_t hash = hash_object(name, length);
    hash ^= parentId;
    return hash % DENTRY_CACHE_SIZE;
}

static uint64_t dentry_cache_add(dentry_t* dentry)
{
    size_t length = strlen(dentry->name);
    uint64_t hash = dentry_hash(dentry->parent->id, dentry->name, length);

    seqlock_write_acquire(&lock);
    for (dentry_t* iter = cache[hash]; iter != NULL; iter = iter->next)
    {
        if (iter->parent == dentry->parent && iter->name[length] == '\0' &&
            memcmp(iter->name, dentry->name, length) == 0)
        {
            if (REF_COUNT(iter) > 0)
            {
                seqlock_write_release(&lock);
                errno = EEXIST;
                return ERR;
            }
        }
    }
    dentry->next = cache[hash];
    cache[hash] = dentry;
    seqlock_write_release(&lock);

    return 0;
}

static void dentry_cache_remove(dentry_t* dentry)
{
    uint64_t hash = dentry_hash(dentry->parent->id, dentry->name, strlen(dentry->name));

    seqlock_write_acquire(&lock);
    dentry_t** curr = &cache[hash];
    while (*curr != NULL)
    {
        if (*curr == dentry)
        {
            *curr = dentry->next;
            break;
        }
        curr = &(*curr)->next;
    }
    seqlock_write_release(&lock);
}

static void dentry_free(dentry_t* dentry)
{
    if (dentry->parent != NULL)
    {
        dentry_cache_remove(dentry);

        if (!DENTRY_IS_ROOT(dentry))
        {
            assert(dentry->parent != NULL);
            assert(dentry->parent->inode != NULL);

            mutex_acquire(&dentry->parent->inode->mutex);
            list_remove(&dentry->parent->children, &dentry->siblingEntry);
            mutex_release(&dentry->parent->inode->mutex);

            UNREF(dentry->parent);
            dentry->parent = NULL;
        }
    }

    if (dentry->ops != NULL && dentry->ops->cleanup != NULL)
    {
        dentry->ops->cleanup(dentry);
    }

    if (dentry->inode != NULL)
    {
        atomic_fetch_sub_explicit(&dentry->inode->dentryCount, 1, memory_order_relaxed);
        UNREF(dentry->inode);
        dentry->inode = NULL;
    }

    UNREF(dentry->superblock);
    dentry->superblock = NULL;

    rcu_call(&dentry->rcu, rcu_call_free, dentry);
}

dentry_t* dentry_new(superblock_t* superblock, dentry_t* parent, const char* name)
{
    if (superblock == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if ((parent == NULL || name == NULL) && ((void*)parent != (void*)name))
    {
        errno = EINVAL;
        return NULL;
    }

    if (name == NULL)
    {
        name = "";
    }
    else
    {
        size_t nameLen = strnlen_s(name, MAX_NAME);
        if (nameLen >= MAX_NAME || nameLen == 0)
        {
            errno = EINVAL;
            return NULL;
        }
    }

    assert(parent == NULL || superblock == parent->superblock);

    dentry_t* dentry = malloc(sizeof(dentry_t));
    if (dentry == NULL)
    {
        return NULL;
    }

    ref_init(&dentry->ref, dentry_free);
    dentry->id = vfs_id_get();
    strncpy(dentry->name, name, MAX_NAME - 1);
    dentry->name[MAX_NAME - 1] = '\0';
    dentry->inode = NULL;
    dentry->parent = parent != NULL ? REF(parent) : dentry;
    list_entry_init(&dentry->siblingEntry);
    list_init(&dentry->children);
    dentry->superblock = REF(superblock);
    dentry->ops = dentry->superblock->dentryOps;
    dentry->private = NULL;
    dentry->next = NULL;
    atomic_init(&dentry->mountCount, 0);
    list_entry_init(&dentry->otherEntry);

    if (dentry_cache_add(dentry) == ERR)
    {
        UNREF(dentry);
        return NULL;
    }

    return dentry;
}

void dentry_remove(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return;
    }

    if (!DENTRY_IS_ROOT(dentry))
    {
        assert(dentry->parent != NULL);

        mutex_acquire(&dentry->parent->inode->mutex);
        list_remove(&dentry->parent->children, &dentry->siblingEntry);
        mutex_release(&dentry->parent->inode->mutex);

        UNREF(dentry->parent);
        dentry->parent = NULL;
    }

    dentry_cache_remove(dentry);
}

dentry_t* dentry_revalidate(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return NULL;
    }

    if (dentry->ops != NULL && dentry->ops->revalidate != NULL)
    {
        if (dentry->ops->revalidate(dentry) == ERR)
        {
            UNREF(dentry);
            return NULL;
        }
    }

    return dentry;
}

dentry_t* dentry_rcu_get(const dentry_t* parent, const char* name, size_t length)
{
    if (parent == NULL || name == NULL || length == 0)
    {
        return NULL;
    }

    uint64_t hash = dentry_hash(parent->id, name, length);
    dentry_t* dentry = NULL;

    uint64_t seq;
    do
    {
        seq = seqlock_read_begin(&lock);
        for (dentry = cache[hash]; dentry != NULL; dentry = dentry->next)
        {
            if (dentry->parent == parent && dentry->name[length] == '\0' && memcmp(dentry->name, name, length) == 0)
            {
                break;
            }
        }
    } while (seqlock_read_retry(&lock, seq));

    if (dentry != NULL && dentry->ops != NULL && dentry->ops->revalidate != NULL)
    {
        if (dentry->ops->revalidate(dentry) == ERR)
        {
            return NULL;
        }
    }

    return dentry;
}

static dentry_t* dentry_get(const dentry_t* parent, const char* name, size_t length)
{
    RCU_READ_SCOPE();
    uint64_t hash = dentry_hash(parent->id, name, length);
    dentry_t* dentry = NULL;

    uint64_t seq;
    do
    {
        seq = seqlock_read_begin(&lock);
        for (dentry = cache[hash]; dentry != NULL; dentry = dentry->next)
        {
            if (dentry->parent == parent && dentry->name[length] == '\0' && memcmp(dentry->name, name, length) == 0)
            {
                break;
            }
        }
    } while (seqlock_read_retry(&lock, seq));

    return REF_TRY(dentry);
}

dentry_t* dentry_lookup(dentry_t* parent, const char* name, size_t length)
{
    if (parent == NULL || name == NULL || length == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    dentry_t* dentry = dentry_get(parent, name, length);
    if (dentry != NULL)
    {
        return dentry_revalidate(dentry);
    }

    if (!DENTRY_IS_DIR(parent))
    {
        errno = ENOENT;
        return NULL;
    }

    char buffer[MAX_NAME];
    strncpy(buffer, name, length);
    buffer[length] = '\0';

    dentry = dentry_new(parent->superblock, parent, buffer);
    if (dentry == NULL)
    {
        if (errno == EEXIST)
        {
            return dentry_revalidate(dentry_get(parent, name, length));
        }
        return NULL;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    inode_t* dir = parent->inode;
    if (dir->ops == NULL || dir->ops->lookup == NULL)
    {
        return dentry_revalidate(dentry); // Leave it negative
    }

    if (dir->ops->lookup(dir, dentry) == ERR)
    {
        UNREF(dentry);
        return NULL;
    }

    return dentry_revalidate(dentry);
}

void dentry_make_positive(dentry_t* dentry, inode_t* inode)
{
    if (dentry == NULL || inode == NULL)
    {
        return;
    }

    atomic_fetch_add_explicit(&inode->dentryCount, 1, memory_order_relaxed);
    dentry->inode = REF(inode);
    if (!DENTRY_IS_ROOT(dentry))
    {
        list_push_back(&dentry->parent->children, &dentry->siblingEntry);
    }
}

bool dentry_iterate_dots(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (ctx->index++ >= ctx->pos)
    {
        if (!ctx->emit(ctx, ".", dentry->inode->number, dentry->inode->type))
        {
            return false;
        }
    }

    if (ctx->index++ >= ctx->pos)
    {
        if (!ctx->emit(ctx, "..", dentry->parent->inode->number, dentry->parent->inode->type))
        {
            return false;
        }
    }

    return true;
}

uint64_t dentry_generic_iterate(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (!dentry_iterate_dots(dentry, ctx))
    {
        return 0;
    }

    dentry_t* child;
    LIST_FOR_EACH(child, &dentry->children, siblingEntry)
    {
        if (ctx->index++ >= ctx->pos)
        {
            assert(DENTRY_IS_POSITIVE(child));
            if (!ctx->emit(ctx, child->name, child->inode->number, child->inode->type))
            {
                return 0;
            }
        }
    }

    return 0;
}
