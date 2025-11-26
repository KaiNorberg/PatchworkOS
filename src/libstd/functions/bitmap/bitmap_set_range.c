#include <sys/bitmap.h>

void bitmap_set_range(bitmap_t* map, uint64_t low, uint64_t high)
{
    if (low >= high || high > map->length)
    {
        return;
    }

    uint64_t firstQwordIdx = low / 64;
    uint64_t firstBitInQword = low % 64;
    uint64_t lastQwordIdx = (high - 1) / 64;
    uint64_t lastBitInQword = (high - 1) % 64;

    if (firstQwordIdx == lastQwordIdx)
    {
        uint64_t mask = (~0ULL << firstBitInQword) & (~0ULL >> (63 - lastBitInQword));
        map->buffer[firstQwordIdx] |= mask;
        return;
    }

    map->buffer[firstQwordIdx] |= (~0ULL << firstBitInQword);

    for (uint64_t i = firstQwordIdx + 1; i < lastQwordIdx; i++)
    {
        map->buffer[i] = ~0ULL;
    }

    map->buffer[lastQwordIdx] |= (~0ULL >> (63 - lastBitInQword));
}