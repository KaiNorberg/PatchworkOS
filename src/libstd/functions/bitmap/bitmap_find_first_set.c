#include <sys/bitmap.h>

uint64_t bitmap_find_first_set(bitmap_t* map, uint64_t startIdx, uint64_t endIdx)
{
    if (startIdx >= map->length)
    {
        return map->length;
    }

    uint64_t startQwordIdx = startIdx / 64;
    uint64_t startBitIdx = startIdx % 64;
    uint64_t endQwordIdx = BITMAP_BITS_TO_QWORDS(MIN(endIdx, map->length));

    while (startQwordIdx < endQwordIdx)
    {
        uint64_t qword = map->buffer[startQwordIdx];
        if (startBitIdx != 0)
        {
            qword &= ~((1ULL << startBitIdx) - 1);
        }

        if (qword != 0)
        {
            return startQwordIdx * 64 + __builtin_ctzll(qword);
        }

        startQwordIdx++;
        startBitIdx = 0;
    }

    return map->length;
}