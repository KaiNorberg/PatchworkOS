#include "pmm_bitmap.h"

#include "log/log.h"

#include "utils/bitmap.h"
#include <assert.h>
#include <sys/math.h>

void pmm_bitmap_init(pmm_bitmap_t* bitmap, void* buffer, uint64_t size, uintptr_t maxAddr)
{
    assert(size >= maxAddr / PAGE_SIZE);
    bitmap_init(&bitmap->bitmap, buffer, size);
    bitmap->free = 0;
    bitmap->total = maxAddr / PAGE_SIZE;
    bitmap->maxAddr = maxAddr;
}

void* pmm_bitmap_alloc(pmm_bitmap_t* bitmap, uint64_t count, uintptr_t maxAddr, uint64_t alignment)
{
    alignment = MAX(ROUND_UP(alignment, PAGE_SIZE), PAGE_SIZE);
    maxAddr = MIN(maxAddr, bitmap->maxAddr);

    uint64_t index =
        bitmap_find_clear_region_and_set(&bitmap->bitmap, count, maxAddr / PAGE_SIZE, alignment / PAGE_SIZE);
    if (index == ERR)
    {
        return NULL;
    }

    bitmap->free -= count;
    return (void*)(index * PAGE_SIZE + PML_HIGHER_HALF_START);
}

void pmm_bitmap_free(pmm_bitmap_t* bitmap, void* address, uint64_t count)
{
    address = (void*)ROUND_DOWN(address, PAGE_SIZE);

    uint64_t index = (uint64_t)PML_HIGHER_TO_LOWER(address) / PAGE_SIZE;
    assert(index < bitmap->maxAddr / PAGE_SIZE);

    bitmap_clear_range(&bitmap->bitmap, index, index + count);
    bitmap->free += count;
}
