#include "kalloc.h"

#include <stdint.h>
#include <stdio.h>

#include "sched/thread.h"
#include "slab.h"
#include "vmm.h"

static slab_t slabs[KALLOC_MAX_SLABS];
static uint8_t lookupTable[KALLOC_MAX_SLAB_SIZE / KALLOC_ALIGN];

static uint64_t usedSlabs = 0;

static lock_t lock = LOCK_CREATE();

void kalloc_init(void)
{
    printf("kalloc: init\n");
    memset(lookupTable, KALLOC_LOOKUP_NONE, sizeof(lookupTable));
}

void* kmalloc(uint64_t size, kalloc_flags_t flags)
{
    LOCK_DEFER(&lock);

    size = ROUND_UP(size, KALLOC_ALIGN);

    if (size >= KALLOC_MAX_SLAB_SIZE || flags & KALLOC_VMM)
    {
        uint64_t allocSize = size + sizeof(object_t);
        uint64_t pageAmount = BYTES_TO_PAGES(allocSize);

        object_t* object = vmm_kernel_map(NULL, NULL, pageAmount, PML_WRITE);
        if (object == NULL)
        {
            return ERRPTR(ENOMEM);
        }
        list_entry_init(&object->entry);
        object->cache = NULL;
        object->magic = SLAB_MAGIC;
        object->freed = true;
        object->dataSize = size;
        return object->data;
    }

    if (lookupTable[size / KALLOC_ALIGN] == KALLOC_LOOKUP_NONE)
    {
        assert(usedSlabs != KALLOC_MAX_SLABS);

        uint64_t slabIndex = usedSlabs++;
        lookupTable[size / KALLOC_ALIGN] = slabIndex;
        slab_init(&slabs[slabIndex], size);
    }

    return slab_alloc(&slabs[lookupTable[size / KALLOC_ALIGN]])->data;
}

void* krealloc(void* oldPtr, uint64_t newSize, kalloc_flags_t flags)
{
    newSize = ROUND_UP(newSize, KALLOC_ALIGN);

    if (oldPtr == NULL)
    {
        return kmalloc(newSize, flags);
    }

    if (newSize == 0)
    {
        kfree(oldPtr);
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

    void* newPtr = kmalloc(newSize, flags);
    if (newPtr == NULL)
    {
        return NULL;
    }

    uint64_t copySize = newSize < object->dataSize ? newSize : object->dataSize;
    memcpy(newPtr, oldPtr, copySize);
    kfree(oldPtr);
    return newPtr;
}

void* kcalloc(uint64_t num, uint64_t size, kalloc_flags_t flags)
{
    uint64_t totalSize = num * size;

    void* ptr = kmalloc(totalSize, flags);
    if (ptr == NULL)
    {
        return ptr;
    }

    memset(ptr, 0, totalSize);
    return ptr;
}

void kfree(void* ptr)
{
    LOCK_DEFER(&lock);

    object_t* object = CONTAINER_OF(ptr, object_t, data);

    if (object->cache == NULL)
    {
        uint64_t allocSize = object->dataSize + sizeof(object_t);
        vmm_kernel_unmap(object, BYTES_TO_PAGES(allocSize));
        return;
    }

    slab_free(object->cache->slab, object);
}

#ifdef TESTING
#include "utils/testing.h"

static uint64_t kalloc_test_single(uint64_t size, uint8_t pattern)
{
    void* ptr = kmalloc(size, KALLOC_NONE);
    if (ptr == NULL)
    {
        printf("kalloc_test_single: Failed to allocate %lu bytes\n", size);
        return ERR;
    }

    memset(ptr, pattern, size);
    for (uint64_t i = 0; i < size; i++)
    {
        if (((uint8_t*)ptr)[i] != pattern)
        {
            printf("kalloc_test_single: Memory corruption detected at offset %lu for size %lu\n", i, size);
            kfree(ptr);
            return ERR;
        }
    }

    kfree(ptr);
    return 0;
}

static uint64_t kalloc_test_multiple(uint64_t numAllocs, uint64_t size, uint8_t pattern)
{
    void* ptrs[numAllocs];
    for (uint64_t i = 0; i < numAllocs; i++)
    {
        ptrs[i] = kmalloc(size, KALLOC_NONE);
        if (ptrs[i] == NULL)
        {
            printf("kalloc_test_multiple: Failed to allocate %lu bytes for allocation %lu\n", size, i);
            for (uint64_t j = 0; j < i; j++)
            {
                kfree(ptrs[j]);
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
                printf("kalloc_test_multiple: Memory corruption detected at offset %lu for allocation %lu, size %lu\n", j, i, size);
                for (uint64_t k = 0; k < numAllocs; k++)
                {
                    kfree(ptrs[k]);
                }
                return ERR;
            }
        }
        kfree(ptrs[i]);
    }
    return 0;
}

static uint64_t kalloc_test_calloc(uint64_t num, uint64_t size)
{
    uint64_t totalSize = num * size;
    void* ptr = kcalloc(num, size, KALLOC_NONE);
    if (ptr == NULL)
    {
        printf("kalloc_test_calloc: Failed to allocate %lu bytes with kcalloc\n", totalSize);
        return ERR;
    }

    for (uint64_t i = 0; i < totalSize; i++)
    {
        if (((uint8_t*)ptr)[i] != 0)
        {
            printf("kalloc_test_calloc: Memory not zero-initialized at offset %lu\n", i);
            kfree(ptr);
            return ERR;
        }
    }
    kfree(ptr);
    return 0;
}

static uint64_t kalloc_test_realloc(uint64_t initialSize, uint64_t newSize, uint8_t pattern)
{
    void* ptr = kmalloc(initialSize, KALLOC_NONE);
    if (ptr == NULL)
    {
        printf("kalloc_test_realloc: Failed to allocate initial %lu bytes\n", initialSize);
        return ERR;
    }
    memset(ptr, pattern, initialSize);

    void* newPtr = krealloc(ptr, newSize, KALLOC_NONE);
    if (newPtr == NULL)
    {
        printf("kalloc_test_realloc: Failed to reallocate to %lu bytes\n", newSize);
        kfree(ptr);
        return ERR;
    }

    uint64_t verifySize = initialSize < newSize ? initialSize : newSize;
    for (uint64_t i = 0; i < verifySize; i++)
    {
        if (((uint8_t*)newPtr)[i] != pattern)
        {
            printf("kalloc_test_realloc: Memory corruption after realloc at offset %lu\n", i);
            kfree(newPtr);
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
                printf("kalloc_test_realloc: New memory not filled correctly at offset %lu\n", i);
                kfree(newPtr);
                return ERR;
            }
        }
    }

    kfree(newPtr);
    return 0;
}

TESTING_REGISTER_TEST(kalloc_all_tests)
{
    uint64_t result = 0;

    printf("Running kalloc tests...\n");

    // Test single allocations
    result |= kalloc_test_single(16, 0xAA);
    result |= kalloc_test_single(64, 0xBB);
    result |= kalloc_test_single(256, 0xCC);
    result |= kalloc_test_single(1024, 0xDD); // 1KB
    result |= kalloc_test_single(4096, 0xEE); // 4KB (page size)
    result |= kalloc_test_single(8192, 0xFF); // 8KB (multiple pages)

    // Test multiple allocations
    result |= kalloc_test_multiple(10, 32, 0x11);
    result |= kalloc_test_multiple(5, 512, 0x22);
    result |= kalloc_test_multiple(3, 4096, 0x33);

    // Test kcalloc
    result |= kalloc_test_calloc(10, 10);
    result |= kalloc_test_calloc(1, 4096);

    // Test krealloc
    result |= kalloc_test_realloc(100, 200, 0x44); // Grow
    result |= kalloc_test_realloc(200, 100, 0x55); // Shrink
    result |= kalloc_test_realloc(50, 50, 0x66);   // Same size
    result |= kalloc_test_realloc(4096, 8192, 0x77); // Grow page-sized
    result |= kalloc_test_realloc(8192, 4096, 0x88); // Shrink page-sized

    return result;
}

#endif
