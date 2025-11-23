#include <sys/bitmap.h>

void bitmap_clear(bitmap_t* map, uint64_t index)
{
    if (index >= map->length)
    {
        return;
    }

    uint64_t qwordIdx = index / 64;
    uint64_t bitInQword = index % 64;
    map->buffer[qwordIdx] &= ~(1ULL << bitInQword);
    map->firstZeroIdx = MIN(map->firstZeroIdx, index);
}