#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/log/log.h>
#include <kernel/mem/cache.h>
#include <kernel/mem/pmm.h>

#include <errno.h>
#include <kernel/mem/vmm.h>
#include <kernel/sync/lock.h>
#include <stdlib.h>
#include <sys/list.h>
#include <sys/math.h>

static cache_slab_t* cache_slab_new(cache_t* cache)
{
    cache_slab_t* slab = vmm_alloc(NULL, NULL, CACHE_SLAB_PAGES * PAGE_SIZE, CACHE_SLAB_PAGES * PAGE_SIZE,
        PML_PRESENT | PML_WRITE | PML_GLOBAL, VMM_ALLOC_FAIL_IF_MAPPED);
    if (slab == NULL)
    {
        return NULL;
    }
    list_entry_init(&slab->entry);
    slab->owner = CPU_ID_INVALID;
    slab->freeCount = cache->layout.amount;
    slab->firstFree = 0;
    lock_init(&slab->lock);
    slab->cache = cache;
    slab->objects = (void*)((uintptr_t)slab + cache->layout.start);

    if (cache->ctor != NULL)
    {
        for (uint32_t i = 0; i < cache->layout.amount; i++)
        {
            cache->ctor((void*)((uintptr_t)slab->objects + (i * cache->layout.step)));
        }
    }

    for (uint32_t i = 0; i < cache->layout.amount - 1; i++)
    {
        slab->bufctl[i] = i + 1;
    }
    slab->bufctl[cache->layout.amount - 1] = CACHE_BUFCTL_END;

    return slab;
}

static void cache_slab_destroy(cache_slab_t* slab)
{
    if (slab->cache->dtor != NULL)
    {
        for (uint32_t i = 0; i < slab->cache->layout.amount; i++)
        {
            slab->cache->dtor((void*)((uintptr_t)slab->objects + (i * slab->cache->layout.step)));
        }
    }
    vmm_unmap(NULL, slab, CACHE_SLAB_PAGES * PAGE_SIZE);
}

static inline void* cache_slab_alloc(cache_slab_t* slab)
{
    if (slab->freeCount == 0)
    {
        return NULL;
    }

    void* object = (void*)((uintptr_t)slab->objects + (slab->firstFree * slab->cache->layout.step));
    slab->firstFree = slab->bufctl[slab->firstFree];
    slab->freeCount--;
    return object;
}

static inline void cache_slab_free(cache_slab_t* slab, void* ptr)
{
    uintptr_t offset = (uintptr_t)ptr - (uintptr_t)slab->objects;
    uint32_t index = offset >> slab->cache->layout.stepShift;

    slab->bufctl[index] = slab->firstFree;
    slab->firstFree = index;
    slab->freeCount++;
}

static inline void cache_precalc_layout(cache_t* cache)
{
    cache->layout.step = ROUND_UP(cache->size, cache->alignment);
    if (!IS_POW2(cache->layout.step))
    {
        cache->layout.step = next_pow2(cache->layout.step);
    }
    cache->layout.stepShift = __builtin_ctz(cache->layout.step);

    uint32_t available = CACHE_SLAB_PAGES * PAGE_SIZE - sizeof(cache_slab_t);
    cache->layout.amount = available / (cache->size + sizeof(cache_bufctl_t));

    while (cache->layout.amount > 0)
    {
        cache->layout.start =
            ROUND_UP(sizeof(cache_slab_t) + cache->layout.amount * sizeof(cache_bufctl_t), cache->alignment);

        if (cache->layout.start + (cache->layout.amount * cache->layout.step) <= CACHE_SLAB_PAGES * PAGE_SIZE)
        {
            break;
        }
        cache->layout.amount--;
    }

    assert(cache->layout.amount != 0);
}

void* cache_alloc(cache_t* cache)
{
    if (cache == NULL)
    {
        return NULL;
    }

    CLI_SCOPE();

    if (cache->cpus[SELF->id].active != NULL)
    {
        cache_slab_t* active = cache->cpus[SELF->id].active;
        lock_acquire(&active->lock);

        void* result = cache_slab_alloc(active);
        if (result != NULL)
        {
            lock_release(&active->lock);
            return result;
        }
        lock_release(&active->lock);

        lock_acquire(&cache->lock);
        lock_acquire(&active->lock);

        if (active->freeCount > 0)
        {
            result = cache_slab_alloc(active);
            lock_release(&active->lock);
            lock_release(&cache->lock);
            return result;
        }

        active->owner = CPU_ID_INVALID;
        cache->cpus[SELF->id].active = NULL;
        list_remove(&active->entry);
        list_push_back(&cache->full, &active->entry);
        lock_release(&active->lock);
        goto allocate;
    }

    lock_acquire(&cache->lock);
allocate:

    if (cache->layout.amount == 0)
    {
        cache_precalc_layout(cache);
        LOG_DEBUG("cache '%s' layout: step=%u, amount=%u, start=%u\n", cache->name, cache->layout.step,
            cache->layout.amount, cache->layout.start);
    }

    cache_slab_t* slab;
    LIST_FOR_EACH(slab, &cache->partial, entry)
    {
        if (slab->owner == CPU_ID_INVALID)
        {
            list_remove(&slab->entry);
            list_push_back(&cache->partial, &slab->entry);
            slab->owner = SELF->id;
            cache->cpus[SELF->id].active = slab;
            lock_release(&cache->lock);

            LOCK_SCOPE(&slab->lock);
            return cache_slab_alloc(slab);
        }
    }

    if (!list_is_empty(&cache->free))
    {
        slab = CONTAINER_OF(list_pop_front(&cache->free), cache_slab_t, entry);
        cache->freeCount--;
        list_push_back(&cache->partial, &slab->entry);
        slab->owner = SELF->id;
        cache->cpus[SELF->id].active = slab;
        lock_release(&cache->lock);

        LOCK_SCOPE(&slab->lock);
        return cache_slab_alloc(slab);
    }

    slab = cache_slab_new(cache);
    if (slab == NULL)
    {
        lock_release(&cache->lock);
        return NULL;
    }

    list_push_back(&cache->partial, &slab->entry);
    slab->owner = SELF->id;
    cache->cpus[SELF->id].active = slab;
    lock_release(&cache->lock);

    LOCK_SCOPE(&slab->lock);
    return cache_slab_alloc(slab);
}

void cache_free(void* ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    cache_slab_t* slab = (cache_slab_t*)ROUND_DOWN((uintptr_t)ptr, CACHE_SLAB_PAGES * PAGE_SIZE);
    cache_t* cache = slab->cache;

    lock_acquire(&slab->lock);
    bool wasFull = (slab->freeCount == 0);
    cache_slab_free(slab, ptr);
    bool isEmpty = (slab->freeCount == cache->layout.amount);
    lock_release(&slab->lock);

    if (wasFull || isEmpty)
    {
        lock_acquire(&cache->lock);

        if (wasFull)
        {
            list_remove(&slab->entry);
            list_push_back(&cache->partial, &slab->entry);
        }

        if (isEmpty && slab->owner == CPU_ID_INVALID)
        {
            list_remove(&slab->entry);
            list_push_back(&cache->free, &slab->entry);
            cache->freeCount++;
            while (cache->freeCount > CACHE_LIMIT)
            {
                cache_slab_t* freeSlab = CONTAINER_OF(list_pop_front(&cache->free), cache_slab_t, entry);
                cache->freeCount--;
                cache_slab_destroy(freeSlab);
            }
        }

        lock_release(&cache->lock);
    }
}

#ifdef _TESTING_

#include <kernel/sched/clock.h>
#include <kernel/utils/test.h>

static cache_t testCache = CACHE_CREATE(testCache, "test", 100, CACHE_LINE, NULL, NULL);

TEST_DEFINE(cache)
{
    void* ptr1 = cache_alloc(&testCache);
    TEST_ASSERT(ptr1 != NULL);
    TEST_ASSERT(((uintptr_t)ptr1 & 7) == 0);

    void* ptr2 = cache_alloc(&testCache);
    TEST_ASSERT(ptr2 != NULL);
    TEST_ASSERT(ptr1 != ptr2);

    cache_free(ptr1);
    cache_free(ptr2);

    return 0;
}

#endif