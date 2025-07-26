#include "slab.h"

#include "sched/thread.h"

static cache_t* cache_new(slab_t* slab, uint64_t objectSize, uint64_t size)
{
    cache_t* cache = vmm_kernel_map(NULL, NULL, BYTES_TO_PAGES(size), PML_WRITE);
    if (cache == NULL)
    {
        return NULL;
    }
    list_entry_init(&cache->entry);
    list_init(&cache->freeList);
    cache->slab = slab;

    uint64_t availSize = size - sizeof(cache_t);
    uint64_t totalFileectSize = sizeof(object_t) + objectSize;
    uint64_t maxFileects = availSize / totalFileectSize;

    cache->objectCount = maxFileects;
    cache->freeCount = maxFileects;

    uint8_t* ptr = cache->buffer;
    for (uint64_t i = 0; i < maxFileects; i++)
    {
        object_t* object = (object_t*)ptr;
        list_entry_init(&object->entry);
        object->cache = cache;
        object->magic = SLAB_MAGIC;
        object->freed = true;
        object->dataSize = objectSize;

        list_push(&cache->freeList, &object->entry);
        ptr += totalFileectSize;
    }

    return cache;
}

static uint64_t slab_find_optimal_cache_size(uint64_t objectSize, uint64_t minSize, uint64_t maxSize)
{
    uint64_t bestSize = 0;
    uint64_t bestEfficiencyNumerator = 0;
    uint64_t bestEfficiencyDenominator = 1;
    uint64_t objectStructSize = sizeof(object_t) + objectSize;

    for (uint64_t size = minSize; size <= maxSize; size += PAGE_SIZE)
    {
        uint64_t availSize = size - sizeof(cache_t);
        uint64_t maxFileects = availSize / objectStructSize;
        uint64_t usedBytes = maxFileects * objectStructSize + sizeof(cache_t);

        if (usedBytes * bestEfficiencyDenominator > bestEfficiencyNumerator * size)
        {
            bestSize = size;
            bestEfficiencyNumerator = usedBytes;
            bestEfficiencyDenominator = size;
        }
    }

    return bestSize;
}

void slab_init(slab_t* slab, uint64_t objectSize)
{
    list_init(&slab->emptyCaches);
    list_init(&slab->partialCaches);
    list_init(&slab->fullCaches);
    slab->emptyCacheCount = 0;
    slab->objectSize = objectSize;
    slab->optimalCacheSize = slab_find_optimal_cache_size(objectSize,
        ROUND_UP(CACHE_MIN_LENGTH * objectSize, PAGE_SIZE), ROUND_UP(CACHE_MAX_LENGTH * objectSize, PAGE_SIZE));

    assert(slab->optimalCacheSize >= PAGE_SIZE);
}

object_t* slab_alloc(slab_t* slab)
{    
    cache_t* cache;

    if (!list_is_empty(&slab->partialCaches))
    {
        cache = CONTAINER_OF(list_first(&slab->partialCaches), cache_t, entry);
        list_remove(&slab->partialCaches, &cache->entry);
    }
    else if (!list_is_empty(&slab->emptyCaches))
    {
        cache = CONTAINER_OF(list_first(&slab->emptyCaches), cache_t, entry);
        list_remove(&slab->emptyCaches, &cache->entry);
        slab->emptyCacheCount--;
    }
    else
    {
        cache = cache_new(slab, slab->objectSize, slab->optimalCacheSize);
        if (cache == NULL)
        {
            errno = ENOMEM;
            return NULL;
        }
        assert(cache->freeCount == cache->objectCount);
    }

    list_entry_t* entry = list_pop(&cache->freeList);
    assert(entry != NULL);
    
    object_t* object = CONTAINER_OF(entry, object_t, entry);
    cache->freeCount--;
    
    assert(object->magic == SLAB_MAGIC);
    assert(object->freed == true);
    assert(object->cache == cache);
    assert(object->dataSize == slab->objectSize);
    
    object->freed = false;
    
    if (cache->freeCount == 0)
    {
        list_push(&slab->fullCaches, &cache->entry);
        assert(list_is_empty(&cache->freeList));
    }
    else
    {
        list_push(&slab->partialCaches, &cache->entry);
        assert(!list_is_empty(&cache->freeList));
        assert(list_length(&cache->freeList) == cache->freeCount);
    }

    return object;
}

void slab_free(slab_t* slab, object_t* object)
{
    assert(object->magic == SLAB_MAGIC && "magic number mismatch");
    assert(!object->freed && "double free");

    object->freed = true;

    cache_t* cache = object->cache;
    if (cache->freeCount == 0)
    {
        assert(list_contains_entry(&slab->fullCaches, &cache->entry));
        list_remove(&slab->fullCaches, &cache->entry);
    }
    else
    {
        assert(list_contains_entry(&slab->partialCaches, &cache->entry));
        list_remove(&slab->partialCaches, &cache->entry);
    }

    list_push(&cache->freeList, &object->entry);
    cache->freeCount++;
    
    assert(cache->freeCount <= cache->objectCount);
    assert(list_length(&cache->freeList) == cache->freeCount);

    if (cache->freeCount == cache->objectCount)
    {
        if (slab->emptyCacheCount >= SLAB_MAX_EMPTY_CACHES)
        {
            assert(list_length(&cache->freeList) == cache->objectCount);
            vmm_kernel_unmap(cache, BYTES_TO_PAGES(slab->optimalCacheSize));
        }
        else
        {
            list_push(&slab->emptyCaches, &cache->entry);
            slab->emptyCacheCount++;
            assert(slab->emptyCacheCount == list_length(&slab->emptyCaches));
        }
    }
    else
    {
        list_push(&slab->partialCaches, &cache->entry);
        assert(cache->freeCount > 0 && cache->freeCount < cache->objectCount);
    }
}
