#pragma once

#include "utils/utils.h"
#include <stdint.h>
#include <sys/math.h>

typedef struct
{
    uint64_t firstZeroIdx;
    uint64_t length;
    uint64_t* buffer;
} bitmap_t;

#define BITMAP_BITS_TO_QWORDS(bits) (((bits) + 63) / 64)
#define BITMAP_BITS_TO_BYTES(bits) (BITMAP_BITS_TO_QWORDS(bits) * sizeof(uint64_t))
#define BITMAP_QWORDS_TO_BITS(qwords) ((qwords) * 64)

#define BITMAP_FOR_EACH_SET(idx, map) \
    for (uint64_t qwordIdx = 0; qwordIdx < BITMAP_BITS_TO_QWORDS((map)->length); qwordIdx++) \
        for (uint64_t tempQword = (map)->buffer[qwordIdx]; tempQword != 0; tempQword &= (tempQword - 1)) \
            if (({ \
                    uint64_t bit = __builtin_ctzll(tempQword); \
                    *(idx) = qwordIdx * 64 + bit; \
                    *(idx) < (map)->length; \
                }))

static inline void bitmap_init(bitmap_t* map, void* buffer, uint64_t length)
{
    map->firstZeroIdx = 0;
    map->length = length;
    map->buffer = (uint64_t*)buffer;
}

static inline bool bitmap_is_set(bitmap_t* map, uint64_t idx)
{
    uint64_t qwordIdx = idx / 64;
    uint64_t bitInQword = idx % 64;
    return (map->buffer[qwordIdx] & (1ULL << bitInQword));
}

static inline void bitmap_set(bitmap_t* map, uint64_t low, uint64_t high)
{
    for (uint64_t i = low; i < high; i++)
    {
        uint64_t qwordIdx = i / 64;
        uint64_t bitInQword = i % 64;
        map->buffer[qwordIdx] |= (1ULL << bitInQword);
    }
}

static inline uint64_t bitmap_find_clear_region_and_set(bitmap_t* map, uint64_t length, uintptr_t maxIdx,
    uint64_t align)
{
    for (uint64_t i = map->firstZeroIdx; i < maxIdx; i += align)
    {
        if (!bitmap_is_set(map, i))
        {
            uint64_t j = i + 1;
            for (; j < maxIdx; j++)
            {
                if (j - i == length)
                {
                    bitmap_set(map, i, j);
                    return i;
                }

                if (bitmap_is_set(map, j))
                {
                    i = MAX(ROUND_UP(j, align), align) - align;
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
        uint64_t qwordIdx = i / 64;
        uint64_t bitInQword = i % 64;
        map->buffer[qwordIdx] &= ~(1ULL << bitInQword);
    }
    map->firstZeroIdx = MIN(map->firstZeroIdx, low);
}

static inline uint64_t bitmap_sum(bitmap_t* map, uint64_t low, uint64_t high)
{
    if (low >= high)
    {
        return 0;
    }
    if (high > map->length)
    {
        high = map->length;
    }
    if (low >= map->length)
    {
        return 0;
    }

    uint64_t sum = 0;

    uint64_t firstQwordIdx = low / 64;
    uint64_t firstBitInQword = low % 64;

    uint64_t lastQwordIdx = (high - 1) / 64;
    uint64_t lastBitInQword = (high - 1) % 64;

    if (firstQwordIdx == lastQwordIdx)
    {
        uint64_t qword = map->buffer[firstQwordIdx];
        uint64_t startMask = ~((1ULL << firstBitInQword) - 1);
        uint64_t endMask = (1ULL << (lastBitInQword + 1)) - 1;
        sum += __builtin_popcountll(qword & startMask & endMask);
        return sum;
    }

    if (firstBitInQword != 0)
    {
        uint64_t qword = map->buffer[firstQwordIdx];
        uint64_t mask = ~((1ULL << firstBitInQword) - 1);
        sum += __builtin_popcountll(qword & mask);
        firstQwordIdx++;
    }

    for (uint64_t i = firstQwordIdx; i < lastQwordIdx; ++i)
    {
        sum += __builtin_popcountll(map->buffer[i]);
    }

    if (lastBitInQword != 63)
    {
        uint64_t qword = map->buffer[lastQwordIdx];
        uint64_t mask = (1ULL << (lastBitInQword + 1)) - 1;
        sum += __builtin_popcountll(qword & mask);
    }

    return sum;
}

static inline uint64_t bitmap_find_first_clear(bitmap_t* map)
{
    uint64_t qwordIdx = map->firstZeroIdx / 64;
    uint64_t bitIdx = map->firstZeroIdx % 64;
    uint64_t endQwordIdx = BITMAP_BITS_TO_QWORDS(map->length);

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

static inline uint64_t bitmap_find_first_set(bitmap_t* map)
{
    uint64_t endQwordIdx = BITMAP_BITS_TO_QWORDS(map->length);

    for (uint64_t i = 0; i < endQwordIdx; ++i)
    {
        if (map->buffer[i] != 0ULL)
        {
            return i * 64 + __builtin_ctzll(map->buffer[i]);
        }
    }

    return map->length;
}