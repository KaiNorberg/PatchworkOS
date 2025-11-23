#include <sys/bitmap.h>

uint64_t bitmap_find_first_clear(bitmap_t* map, uint64_t startIdx, uint64_t endIdx)
{
    if (map->firstZeroIdx >= map->length)
    {
        return map->length;
    }

    startIdx = MAX(startIdx, map->firstZeroIdx);
    uint64_t qwordIdx = startIdx / 64;
    uint64_t bitIdx = startIdx % 64;
    uint64_t endQwordIdx = BITMAP_BITS_TO_QWORDS(MIN(endIdx, map->length));

    if (bitIdx != 0)
    {
        uint64_t qword = map->buffer[qwordIdx];
        uint64_t maskedQword = qword | ((1ULL << bitIdx) - 1);
        if (maskedQword != ~0ULL)
        {
            return qwordIdx * 64 + __builtin_ctzll(~maskedQword);
        }
        qwordIdx++;
    }

    for (uint64_t i = qwordIdx; i < endQwordIdx; ++i)
    {
        if (map->buffer[i] != ~0ULL)
        {
            return i * 64 + __builtin_ctzll(~map->buffer[i]);
        }
    }

    return map->length;
}