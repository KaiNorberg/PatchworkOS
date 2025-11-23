#include <sys/bitmap.h>

bool bitmap_is_set(bitmap_t* map, uint64_t idx)
{
    if (idx >= map->length)
    {
        return false;
    }

    uint64_t qwordIdx = idx / 64;
    uint64_t bitInQword = idx % 64;
    return (map->buffer[qwordIdx] & (1ULL << bitInQword));
}