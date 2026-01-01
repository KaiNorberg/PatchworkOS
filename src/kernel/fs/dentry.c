#include <kernel/fs/dentry.h>

#include <kernel/sync/seqlock.h>
#include <stdio.h>

#include <kernel/fs/inode.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>

#include <stdlib.h>

static map_t dentryCache = MAP_CREATE();
static rwlock_t dentryCacheLock = RWLOCK_CREATE();

static map_key_t dentry_cache_key(dentry_id_t parentId, const char* name)
{
    struct
    {
        dentry_id_t parentId;
        char name[MAX_NAME];
    } buffer;
    buffer.parentId = parentId;
    memset(buffer.name, 0, sizeof(buffer.name));
    strncpy_s(buffer.name, sizeof(buffer.name), name, MAX_NAME - 1);

    return map_key_buffer(&buffer, sizeof(buffer));
}

static uint64_t dentry_cache_add(dentry_t* dentry)
{
    map_key_t key = dentry_cache_key(dentry->parent->id, dentry->name);

    RWLOCK_WRITE_SCOPE(&dentryCacheLock);
    if (map_insert(&dentryCache, &key, &dentry->mapEntry) == ERR)
    {
        return ERR;
    }

    return 0;
}

static void dentry_cache_remove(dentry_t* dentry)
{
    RWLOCK_WRITE_SCOPE(&dentryCacheLock);
    map_remove(&dentryCache, &dentry->mapEntry);
}

static dentry_t* dentry_cache_get(map_key_t* key)
{
    RWLOCK_READ_SCOPE(&dentryCacheLock);

    dentry_t* dentry = CONTAINER_OF_SAFE(map_get(&dentryCache, key), dentry_t, mapEntry);
    if (dentry == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    if (atomic_load(&dentry->ref.count) == 0) // Is currently being removed
    {
        errno = ENOENT;
        return NULL;
    }

    return REF(dentry);
}

static void dentry_free(dentry_t* dentry)
{
    if (dentry == NULL)
    {
        return;
    }

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

    free(dentry);
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

    dentry_t* dentry = malloc(sizeof(dentry_t));
    if (dentry == NULL)
    {
        return NULL;
    }

    ref_init(&dentry->ref, dentry_free);
    map_entry_init(&dentry->mapEntry);
    dentry->id = vfs_id_get();
    strncpy(dentry->name, name, MAX_NAME - 1);
    dentry->name[MAX_NAME - 1] = '\0';
    dentry->inode = NULL;
    dentry->parent = parent != NULL ? REF(parent) : dentry;
    list_entry_init(&dentry->siblingEntry);
    list_init(&dentry->children);
    dentry->superblock = REF(superblock);
    dentry->ops = dentry->superblock != NULL ? dentry->superblock->dentryOps : NULL;
    dentry->private = NULL;
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

dentry_t* dentry_get(const dentry_t* parent, const char* name)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    map_key_t key = dentry_cache_key(parent->id, name);
    return dentry_cache_get(&key);
}

dentry_t* dentry_lookup(const path_t* parent, const char* name)
{
    if (!PATH_IS_VALID(parent) || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    map_key_t key = dentry_cache_key(parent->dentry->id, name);
    dentry_t* dentry = dentry_cache_get(&key);
    if (dentry != NULL)
    {
        return dentry;
    }

    if (!DENTRY_IS_DIR(parent->dentry))
    {
        errno = ENOENT;
        return NULL;
    }

    dentry = dentry_new(parent->dentry->superblock, parent->dentry, name);
    if (dentry == NULL)
    {
        if (errno == EEXIST)
        {
            return dentry_cache_get(&key);
        }
        return NULL;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    inode_t* dir = parent->dentry->inode;
    if (dir->ops == NULL || dir->ops->lookup == NULL)
    {
        return dentry; // Leave it negative
    }

    if (dir->ops->lookup(dir, dentry) == ERR)
    {
        UNREF(dentry);
        return NULL;
    }

    return dentry;
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
