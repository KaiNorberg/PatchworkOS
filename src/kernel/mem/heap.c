#include "heap.h"

#include "log/log.h"
#include "sched/thread.h"
#include "slab.h"
#include "vmm.h"

#include <stdint.h>

static slab_t slabs[HEAP_MAX_SLABS];
static uint8_t lookupTable[HEAP_MAX_SLAB_SIZE / HEAP_ALIGN];

static uint64_t usedSlabs = 0;

static lock_t lock = LOCK_CREATE();

extern uint64_t _kernelEnd;

void heap_init(void)
{
    LOG_INFO("heap: init\n");
    memset(lookupTable, HEAP_LOOKUP_NONE, sizeof(lookupTable));
}

void* heap_alloc(uint64_t size, heap_flags_t flags)
{
    LOCK_SCOPE(&lock);

    size = ROUND_UP(size, HEAP_ALIGN);

    if (size >= HEAP_MAX_SLAB_SIZE || flags & HEAP_VMM)
    {
        uint64_t allocSize = size + sizeof(object_t);
        uint64_t pageAmount = BYTES_TO_PAGES(allocSize);

        object_t* object = vmm_kernel_map(NULL, NULL, pageAmount, PML_WRITE);
        if (object == NULL)
        {
            errno = ENOMEM;
            return NULL;
        }
        list_entry_init(&object->entry);
        object->cache = NULL;
        object->magic = SLAB_MAGIC;
        object->freed = false;
        object->dataSize = size;
        return object->data;
    }

    if (lookupTable[size / HEAP_ALIGN] == HEAP_LOOKUP_NONE)
    {
        assert(usedSlabs != HEAP_MAX_SLABS);

        uint64_t slabIndex = usedSlabs++;
        lookupTable[size / HEAP_ALIGN] = slabIndex;
        slab_init(&slabs[slabIndex], size);
    }

    object_t* object = slab_alloc(&slabs[lookupTable[size / HEAP_ALIGN]]);
    if (object == NULL)
    {
        return NULL;
    }

#ifndef NDEBUG
    memset32(object->data, HEAP_ALLOC_POISON, object->dataSize / sizeof(uint32_t));
#endif

    return object->data;
}

void* heap_realloc(void* oldPtr, uint64_t newSize, heap_flags_t flags)
{
    newSize = ROUND_UP(newSize, HEAP_ALIGN);

    if (oldPtr == NULL)
    {
        return heap_alloc(newSize, flags);
    }

    if (newSize == 0)
    {
        heap_free(oldPtr);
        return NULL;
    }

    object_t* object = CONTAINER_OF(oldPtr, object_t, data);

    if (object->cache == NULL)
    {
        uint64_t oldAllocSize = object->dataSize + sizeof(object_t);
        uint64_t newAllocSize = newSize + sizeof(object_t);
        uint64_t oldPages = BYTES_TO_PAGES(oldAllocSize);
        uint64_t newPages = BYTES_TO_PAGES(newAllocSize);

        if (newPages <= oldPages)
        {
            object->dataSize = newSize;
            return oldPtr;
        }
    }
    else if (newSize <= object->dataSize)
    {
        return oldPtr;
    }

    void* newPtr = heap_alloc(newSize, flags);
    if (newPtr == NULL)
    {
        return NULL;
    }

    uint64_t copySize = newSize < object->dataSize ? newSize : object->dataSize;
    memcpy(newPtr, oldPtr, copySize);
    heap_free(oldPtr);
    return newPtr;
}

void* heap_calloc(uint64_t num, uint64_t size, heap_flags_t flags)
{
    uint64_t totalSize = num * size;

    void* ptr = heap_alloc(totalSize, flags);
    if (ptr == NULL)
    {
        return ptr;
    }

    memset(ptr, 0, totalSize);
    return ptr;
}

void heap_free(void* ptr)
{
    assert(ptr > (void*)&_kernelEnd);

    LOCK_SCOPE(&lock);

    object_t* object = CONTAINER_OF(ptr, object_t, data);

    if (object->cache == NULL)
    {
        uint64_t allocSize = object->dataSize + sizeof(object_t);
        vmm_kernel_unmap(object, BYTES_TO_PAGES(allocSize));
        return;
    }

#ifndef NDEBUG
    memset32(object->data, HEAP_FREE_POISON, object->dataSize / sizeof(uint32_t));
#endif

    slab_free(object->cache->slab, object);
}

#ifndef NDEBUG
#include "utils/testing.h"

static uint64_t heap_test_single(uint64_t size, uint8_t pattern)
{
    void* ptr = heap_alloc(size, HEAP_NONE);
    if (ptr == NULL)
    {
        LOG_ERR("heap_test_single: Failed to allocate %lu bytes\n", size);
        return ERR;
    }

    memset(ptr, pattern, size);
    for (uint64_t i = 0; i < size; i++)
    {
        if (((uint8_t*)ptr)[i] != pattern)
        {
            LOG_ERR("heap_test_single: Memory corruption detected at offset %lu for size %lu\n", i, size);
            heap_free(ptr);
            return ERR;
        }
    }

    heap_free(ptr);
    return 0;
}

static uint64_t heap_test_multiple(uint64_t numAllocs, uint64_t size, uint8_t pattern)
{
    void* ptrs[numAllocs];
    for (uint64_t i = 0; i < numAllocs; i++)
    {
        ptrs[i] = heap_alloc(size, HEAP_NONE);
        if (ptrs[i] == NULL)
        {
            LOG_ERR("heap_test_multiple: Failed to allocate %lu bytes for allocation %lu\n", size, i);
            for (uint64_t j = 0; j < i; j++)
            {
                heap_free(ptrs[j]);
            }
            return ERR;
        }
        memset(ptrs[i], pattern, size);
    }

    for (uint64_t i = 0; i < numAllocs; i++)
    {
        for (uint64_t j = 0; j < size; j++)
        {
            if (((uint8_t*)ptrs[i])[j] != pattern)
            {
                LOG_ERR("heap_test_multiple: Memory corruption detected at offset %lu for allocation %lu, size %lu\n",
                    j, i, size);
                for (uint64_t k = 0; k < numAllocs; k++)
                {
                    heap_free(ptrs[k]);
                }
                return ERR;
            }
        }
        heap_free(ptrs[i]);
    }
    return 0;
}

static uint64_t heap_test_calloc(uint64_t num, uint64_t size)
{
    uint64_t totalSize = num * size;
    void* ptr = heap_calloc(num, size, HEAP_NONE);
    if (ptr == NULL)
    {
        LOG_ERR("heap_test_calloc: Failed to allocate %lu bytes with heap_calloc\n", totalSize);
        return ERR;
    }

    for (uint64_t i = 0; i < totalSize; i++)
    {
        if (((uint8_t*)ptr)[i] != 0)
        {
            LOG_ERR("heap_test_calloc: Memory not zero-initialized at offset %lu\n", i);
            heap_free(ptr);
            return ERR;
        }
    }
    heap_free(ptr);
    return 0;
}

static uint64_t heap_test_realloc(uint64_t initialSize, uint64_t newSize, uint8_t pattern)
{
    void* ptr = heap_alloc(initialSize, HEAP_NONE);
    if (ptr == NULL)
    {
        LOG_ERR("heap_test_realloc: Failed to allocate initial %lu bytes\n", initialSize);
        return ERR;
    }
    memset(ptr, pattern, initialSize);

    void* newPtr = heap_realloc(ptr, newSize, HEAP_NONE);
    if (newPtr == NULL)
    {
        LOG_ERR("heap_test_realloc: Failed to reallocate to %lu bytes\n", newSize);
        heap_free(ptr);
        return ERR;
    }

    uint64_t verifySize = initialSize < newSize ? initialSize : newSize;
    for (uint64_t i = 0; i < verifySize; i++)
    {
        if (((uint8_t*)newPtr)[i] != pattern)
        {
            LOG_ERR("heap_test_realloc: Memory corruption after realloc at offset %lu\n", i);
            heap_free(newPtr);
            return ERR;
        }
    }

    if (newSize > initialSize)
    {
        memset((uint8_t*)newPtr + initialSize, pattern + 1, newSize - initialSize);
        for (uint64_t i = initialSize; i < newSize; i++)
        {
            if (((uint8_t*)newPtr)[i] != pattern + 1)
            {
                LOG_ERR("heap_test_realloc: New memory not filled correctly at offset %lu\n", i);
                heap_free(newPtr);
                return ERR;
            }
        }
    }

    heap_free(newPtr);
    return 0;
}

TESTING_REGISTER_TEST(heap_all_tests)
{
    uint64_t result = 0;

    // Test single allocations
    result |= heap_test_single(16, 0xAA);
    result |= heap_test_single(64, 0xBB);
    result |= heap_test_single(256, 0xCC);
    result |= heap_test_single(1024, 0xDD); // 1KB
    result |= heap_test_single(4096, 0xEE); // 4KB (page size)
    result |= heap_test_single(8192, 0xFF); // 8KB (multiple pages)

    // Test multiple allocations
    result |= heap_test_multiple(10, 32, 0x11);
    result |= heap_test_multiple(5, 512, 0x22);
    result |= heap_test_multiple(3, 4096, 0x33);

    // Test heap_calloc
    result |= heap_test_calloc(10, 10);
    result |= heap_test_calloc(1, 4096);

    // Test heap_realloc
    result |= heap_test_realloc(100, 200, 0x44);   // Grow
    result |= heap_test_realloc(200, 100, 0x55);   // Shrink
    result |= heap_test_realloc(50, 50, 0x66);     // Same size
    result |= heap_test_realloc(4096, 8192, 0x77); // Grow page-sized
    result |= heap_test_realloc(8192, 4096, 0x88); // Shrink page-sized

    return result;
}

#endif
