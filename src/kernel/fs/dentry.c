#include <kernel/fs/dentry.h>

#include <kernel/sync/rcu.h>
#include <kernel/sync/seqlock.h>
#include <stdio.h>

#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/cache.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/seqlock.h>

#include <stdlib.h>
#include <sys/list.h>

typedef struct
{
    const dentry_t* parent;
    const char* name;
    size_t length;
} dentry_key_t;

static bool dentry_cmp(map_entry_t* entry, const void* key)
{
    dentry_t* dentry = CONTAINER_OF(entry, dentry_t, mapEntry);
    const dentry_key_t* k = key;
    return dentry->parent == k->parent && dentry->name[k->length] == '\0' &&
        memcmp(dentry->name, k->name, k->length) == 0;
}

#define DENTRY_MAP_SIZE 4096
static MAP_CREATE(dentryMap, DENTRY_MAP_SIZE, dentry_cmp);
static seqlock_t lock = SEQLOCK_CREATE();

static uint64_t dentry_hash(dentry_id_t parentId, const char* name, size_t length)
{
    uint64_t hash = hash_buffer(name, length);
    hash ^= parentId;
    return hash;
}

static bool dentry_map_add(dentry_t* dentry)
{
    if (dentry->parent == NULL)
    {
        return true;
    }

    size_t length = strlen(dentry->name);
    uint64_t hash = dentry_hash(dentry->parent->id, dentry->name, length);
    dentry_key_t key = {.parent = dentry->parent, .name = dentry->name, .length = length};

    seqlock_write_acquire(&lock);
    map_entry_t* entry = map_find(&dentryMap, &key, hash);
    if (entry != NULL)
    {
        dentry_t* existing = CONTAINER_OF(entry, dentry_t, mapEntry);
        if (REF_COUNT(existing) > 0)
        {
            seqlock_write_release(&lock);
            return false;
        }
    }
    map_insert(&dentryMap, &dentry->mapEntry, hash);
    seqlock_write_release(&lock);

    return true;
}

static void dentry_map_remove(dentry_t* dentry)
{
    if (dentry->parent == NULL)
    {
        return;
    }

    uint64_t hash = dentry_hash(dentry->parent->id, dentry->name, strlen(dentry->name));

    seqlock_write_acquire(&lock);
    map_remove(&dentryMap, &dentry->mapEntry, hash);
    seqlock_write_release(&lock);
}

static void dentry_free(dentry_t* dentry)
{
    dentry_map_remove(dentry);

    if (dentry->parent != NULL)
    {
        assert(dentry->parent->vnode != NULL);

        mutex_acquire(&dentry->parent->vnode->mutex);
        list_remove(&dentry->siblingEntry);
        mutex_release(&dentry->parent->vnode->mutex);

        UNREF(dentry->parent);
        dentry->parent = NULL;
    }

    if (dentry->vnode != NULL)
    {
        UNREF(dentry->vnode);
        dentry->vnode = NULL;
    }

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
    map_entry_init(&dentry->mapEntry);
    atomic_init(&dentry->bindings, 0);
    dentry->rcu = (rcu_entry_t){0};
    list_entry_init(&dentry->entry);
}

static cache_t cache = CACHE_CREATE(cache, "dentry", sizeof(dentry_t), CACHE_LINE, dentry_ctor, NULL);

dentry_t* dentry_new(dentry_t* parent, const char* name)
{
    dentry_t* dentry = cache_alloc(&cache);
    if (dentry == NULL)
    {
        return NULL;
    }

    ref_init(&dentry->ref, dentry_free);
    if (name != NULL)
    {
        strncpy(dentry->name, name, MAX_NAME);
        dentry->name[MAX_NAME - 1] = '\0';
    }
    else
    {
        strncpy(dentry->name, "__root__", MAX_NAME);
        dentry->name[MAX_NAME - 1] = '\0';
    }
    dentry->parent = parent != NULL ? REF(parent) : NULL;

    if (!dentry_map_add(dentry))
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
    dentry_key_t key = {.parent = parent, .name = name, .length = length};

    uint64_t seq;
    do
    {
        seq = seqlock_read_begin(&lock);
        map_entry_t* entry = map_find(&dentryMap, &key, hash);
        if (entry != NULL)
        {
            dentry = CONTAINER_OF(entry, dentry_t, mapEntry);
        }
        else
        {
            dentry = NULL;
        }
    } while (seqlock_read_retry(&lock, seq));

    if (DENTRY_IS_POSITIVE(dentry))
    {
        if (dentry->vnode->cls->access != NULL && !dentry->vnode->cls->access(dentry))
        {
            UNREF(dentry);
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

void dentry_make_positive(dentry_t* dentry, vnode_t* vnode)
{
    if (dentry == NULL || vnode == NULL)
    {
        return;
    }

    dentry->vnode = REF(vnode);
    if (dentry->parent != NULL)
    {
        list_push_back(&dentry->parent->children, &dentry->siblingEntry);
    }
}
