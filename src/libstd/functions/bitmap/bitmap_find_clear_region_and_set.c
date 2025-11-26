#include <sys/bitmap.h>

uint64_t bitmap_find_clear_region_and_set(bitmap_t* map, uint64_t minIdx, uintptr_t maxIdx, uint64_t length,
    uint64_t alignment)
{
    if (length == 0 || minIdx >= maxIdx || maxIdx > map->length)
    {
        return map->length;
    }

    if (alignment == 0)
    {
        alignment = 1;
    }

    uint64_t idx = MAX(minIdx, map->firstZeroIdx);
    idx = ROUND_UP(idx, alignment);

    while (idx <= maxIdx - length)
    {
        uint64_t firstSet = bitmap_find_first_set(map, idx, idx + length);
        if (firstSet >= idx + length)
        {
            bitmap_set_range(map, idx, idx + length);
            return idx;
        }
        idx = ROUND_UP(firstSet + 1, alignment);
    }

    return map->length;
}