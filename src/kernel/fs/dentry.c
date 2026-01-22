#include <kernel/fs/dentry.h>

#include <kernel/sync/rcu.h>
#include <kernel/sync/seqlock.h>
#include <stdio.h>

#include <kernel/fs/vnode.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/cache.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/seqlock.h>

#include <stdlib.h>
#include <sys/list.h>

#define DENTRY_MAP_SIZE 4096
static dentry_t* map[DENTRY_MAP_SIZE] = {NULL};
static seqlock_t lock = SEQLOCK_CREATE();

static uint64_t dentry_hash(dentry_id_t parentId, const char* name, size_t length)
{
    uint64_t hash = hash_object(name, length);
    hash ^= parentId;
    return hash % DENTRY_MAP_SIZE;
}

static uint64_t dentry_map_add(dentry_t* dentry)
{
    size_t length = strlen(dentry->name);
    uint64_t hash = dentry_hash(dentry->parent->id, dentry->name, length);

    seqlock_write_acquire(&lock);
    for (dentry_t* iter = map[hash]; iter != NULL; iter = iter->next)
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
    dentry->next = map[hash];
    map[hash] = dentry;
    seqlock_write_release(&lock);

    return 0;
}

static void dentry_map_remove(dentry_t* dentry)
{
    uint64_t hash = dentry_hash(dentry->parent->id, dentry->name, strlen(dentry->name));

    seqlock_write_acquire(&lock);
    dentry_t** curr = &map[hash];
    while (*curr != NULL)
    {
        if (*curr == dentry)
        {
            *curr = dentry->next;
            dentry->next = NULL;
            break;
        }
        curr = &(*curr)->next;
    }
    seqlock_write_release(&lock);
}

static void dentry_free(dentry_t* dentry)
{
    dentry_map_remove(dentry);

    if (!DENTRY_IS_ROOT(dentry))
    {
        assert(dentry->parent != NULL);
        assert(dentry->parent->vnode != NULL);

        mutex_acquire(&dentry->parent->vnode->mutex);
        list_remove(&dentry->siblingEntry);
        mutex_release(&dentry->parent->vnode->mutex);

        UNREF(dentry->parent);
        dentry->parent = NULL;
    }

    if (dentry->ops != NULL && dentry->ops->cleanup != NULL)
    {
        dentry->ops->cleanup(dentry);
    }
    dentry->data = NULL;

    if (dentry->vnode != NULL)
    {
        atomic_fetch_sub_explicit(&dentry->vnode->dentryCount, 1, memory_order_relaxed);
        UNREF(dentry->vnode);
        dentry->vnode = NULL;
    }

    UNREF(dentry->superblock);
    dentry->superblock = NULL;

    rcu_call(&dentry->rcu, rcu_call_cache_free, dentry);
}

static void dentry_ctor(void* ptr)
{
    dentry_t* dentry = (dentry_t*)ptr;

    dentry->ref = (ref_t){0};
    dentry->id = vfs_id_get();
    dentry->name[0] = '\0';
    dentry->vnode = NULL;
    dentry->parent = NULL;
    list_entry_init(&dentry->siblingEntry);
    list_init(&dentry->children);
    dentry->superblock = NULL;
    dentry->ops = NULL;
    dentry->data = NULL;
    dentry->next = NULL;
    atomic_init(&dentry->mountCount, 0);
    dentry->rcu = (rcu_entry_t){0};
    list_entry_init(&dentry->otherEntry);
}

static cache_t cache = CACHE_CREATE(cache, "dentry", sizeof(dentry_t), CACHE_LINE, dentry_ctor, NULL);

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

    assert(parent == NULL || superblock == parent->superblock);

    dentry_t* dentry = cache_alloc(&cache);
    if (dentry == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    ref_init(&dentry->ref, dentry_free);
    dentry->superblock = REF(superblock);
    dentry->ops = superblock->dentryOps;
    strncpy(dentry->name, name, MAX_NAME);
    dentry->name[MAX_NAME - 1] = '\0';
    dentry->parent = parent != NULL ? REF(parent) : dentry;

    if (dentry_map_add(dentry) == ERR)
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

    dentry_map_remove(dentry);
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
        for (dentry = map[hash]; dentry != NULL; dentry = dentry->next)
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

    return REF_TRY(dentry_rcu_get(parent, name, length));
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
        return dentry;
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
            return dentry_get(parent, name, length);
        }
        return NULL;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    vnode_t* dir = parent->vnode;
    if (dir->ops == NULL || dir->ops->lookup == NULL)
    {
        return dentry; // Leave it as negative.
    }

    if (dir->ops->lookup(dir, dentry) == ERR)
    {
        UNREF(dentry);
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

void dentry_make_positive(dentry_t* dentry, vnode_t* vnode)
{
    if (dentry == NULL || vnode == NULL)
    {
        return;
    }

    atomic_fetch_add_explicit(&vnode->dentryCount, 1, memory_order_relaxed);
    dentry->vnode = REF(vnode);
    if (!DENTRY_IS_ROOT(dentry))
    {
        list_push_back(&dentry->parent->children, &dentry->siblingEntry);
    }
}

bool dentry_iterate_dots(dentry_t* dentry, dir_ctx_t* ctx)
{
    if (ctx->index++ >= ctx->pos)
    {
        if (!ctx->emit(ctx, ".", dentry->vnode->type))
        {
            return false;
        }
    }

    if (ctx->index++ >= ctx->pos)
    {
        if (!ctx->emit(ctx, "..", dentry->parent->vnode->type))
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
            if (!ctx->emit(ctx, child->name, child->vnode->type))
            {
                return 0;
            }
        }
    }

    return 0;
}
