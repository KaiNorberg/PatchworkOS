#pragma once

#include "utils/utils.h"

#include <stdint.h>
#include <sys/math.h>

typedef struct
{
    uint64_t firstZeroIndex;
    uint64_t length;
    uint8_t* buffer;
} bitmap_t;

#define BITMAP_BITS_TO_BYTES(bits) (((bits) + 7) / 8)
#define BITMAP_BYTES_TO_BITS(bytes) ((bytes) * 8)

// The length is the total amount of BITS in the buffer.
static inline void bitmap_init(bitmap_t* map, void* buffer, uint64_t length)
{
    map->firstZeroIndex = 0;
    map->length = length;
    map->buffer = buffer;
}

static inline bool bitmap_is_set(bitmap_t* map, uint64_t index)
{
    return (map->buffer[(index) / 8] & (1ULL << ((index) % 8)));
}

static inline void bitmap_set(bitmap_t* map, uint64_t low, uint64_t high)
{
    for (uint64_t i = low; i < high; i++)
    {
        map->buffer[i / 8] |= (1ULL << (i % 8));
    }
}

static inline uint64_t bitmap_find_clear_region_and_set(bitmap_t* map, uint64_t length, uintptr_t maxIndex,
    uint64_t alignment)
{
    for (uint64_t i = map->firstZeroIndex; i < maxIndex; i += alignment)
    {
        if (!bitmap_is_set(map, i))
        {
            uint64_t j = i + 1;
            for (; j < maxIndex; j++)
            {
                if (j - i == length)
                {
                    bitmap_set(map, i, j);
                    return i;
                }

                if (bitmap_is_set(map, j))
                {
                    i = MAX(ROUND_UP(j, alignment), alignment) - alignment;
                    break;
                }
            }
        }
    }
    return ERR;
}

static inline void bitmap_clear(bitmap_t* map, uint64_t low, uint64_t high)
{
    for (uint64_t i = low; i < high; i++)
    {
        map->buffer[i / 8] &= ~(1ULL << (i % 8));
    }
    map->firstZeroIndex = MIN(map->firstZeroIndex, low);
}

static inline uint64_t bitmap_sum(bitmap_t* map, uint64_t low, uint64_t high)
{
    uint64_t sum = 0;
    for (uint64_t i = low; i < high; i++)
    {
        if (bitmap_is_set(map, i))
        {
            sum++;
        }
    }
    return sum;
}
