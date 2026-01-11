#include <sys/bitmap.h>

bool bitmap_is_empty(bitmap_t* map)
{
    uint64_t fullQwords = map->length / 64;
    for (uint64_t i = 0; i < fullQwords; i++)
    {
        if (map->buffer[i] != 0)
        {
            return false;
        }
    }

    uint64_t remainingBits = map->length % 64;
    if (remainingBits != 0)
    {
        uint64_t mask = (1ULL << remainingBits) - 1;
        if ((map->buffer[fullQwords] & mask) != 0)
        {
            return false;
        }
    }

    return true;
}