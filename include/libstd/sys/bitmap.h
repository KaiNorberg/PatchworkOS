#ifndef _SYS_BITMAP_H
#define _SYS_BITMAP_H 1

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/math.h>

/**
 * @brief Bitmap.
 * @defgroup libstd_sys_bitmap Bitmap
 * @ingroup libstd
 *
 * @{
 */

/**
 * @brief Bitmap structure.
 * @struct bitmap_t
 */
typedef struct
{
    uint64_t firstZeroIdx;
    uint64_t length;
    uint64_t* buffer;
} bitmap_t;

/**
 * @brief Convert number of bits to number of qwords.
 *
 * @param bits Number of bits.
 * @return Number of qwords.
 */
#define BITMAP_BITS_TO_QWORDS(bits) (((bits) + 63) / 64)

/**
 * @brief Convert number of bits to number of bytes.
 *
 * @param bits Number of bits.
 * @return Number of bytes.
 */
#define BITMAP_BITS_TO_BYTES(bits) (BITMAP_BITS_TO_QWORDS(bits) * sizeof(uint64_t))

/**
 * @brief Convert number of qwords to number of bits.
 *
 * @param qwords Number of qwords.
 * @return Number of bits.
 */
#define BITMAP_QWORDS_TO_BITS(qwords) ((qwords) * 64)

/**
 * @brief Iterate over each set bit in the bitmap.
 *
 * @param idx Pointer to store the current index.
 * @param map The bitmap.
 */
#define BITMAP_FOR_EACH_SET(idx, map) \
    for (uint64_t qwordIdx = 0; qwordIdx < BITMAP_BITS_TO_QWORDS((map)->length); qwordIdx++) \
        for (uint64_t tempQword = (map)->buffer[qwordIdx]; tempQword != 0; tempQword &= (tempQword - 1)) \
            if (({ \
                    uint64_t bit = __builtin_ctzll(tempQword); \
                    *(idx) = qwordIdx * 64 + bit; \
                    *(idx) < (map)->length; \
                }))

/**
 * @brief Create a bitmap initializer.
 *
 * @param buffer Pointer to the buffer, must be a multiple of 8 bytes.
 * @param length Length of the bitmap in bits.
 * @return Bitmap initializer.
 */
#define BITMAP_CREATE(buffer, length) {0, length, buffer}

/**
 * @brief Define a bitmap and its buffer.
 *
 * @param name Name of the bitmap.
 * @param bits Length of the bitmap in bits.
 */
#define BITMAP_DEFINE(name, bits) \
    uint8_t name##Buffer[BITMAP_BITS_TO_QWORDS(bits)]; \
    bitmap_t name

/**
 * @brief Initialize a bitmap defined with `BITMAP_DEFINE`.
 *
 * @param name Name of the bitmap.
 * @param bits Length of the bitmap in bits.
 */
#define BITMAP_DEFINE_INIT(name, bits) \
    bitmap_init(&name, name##Buffer, bits);

/**
 * @brief Initialize a bitmap.
 *
 * @param map The bitmap.
 * @param buffer Pointer to the buffer, must be a multiple of 8 bytes.
 * @param length Length of the bitmap in bits.
 */
static inline void bitmap_init(bitmap_t* map, void* buffer, uint64_t length)
{
    map->firstZeroIdx = 0;
    map->length = length;
    map->buffer = (uint64_t*)buffer;
}

/**
 * @brief Check if a bit is set in the bitmap.
 *
 * @param map The bitmap.
 * @param idx Index of the bit to check.
 * @return true if the bit is set, false otherwise.
 */
static inline bool bitmap_is_set(bitmap_t* map, uint64_t idx)
{
    if (idx >= map->length)
    {
        return false;
    }

    uint64_t qwordIdx = idx / 64;
    uint64_t bitInQword = idx % 64;
    return (map->buffer[qwordIdx] & (1ULL << bitInQword));
}

/**
 * @brief Set a bit in the bitmap.
 *
 * @param map Pointer to the bitmap.
 * @param index Index of the bit to set.
 */
static inline void bitmap_set(bitmap_t* map, uint64_t index)
{
    if (index >= map->length)
    {
        return;
    }

    uint64_t qwordIdx = index / 64;
    uint64_t bitInQword = index % 64;
    map->buffer[qwordIdx] |= (1ULL << bitInQword);
}

/**
 * @brief Set a range of bits in the bitmap.
 *
 * @param map Pointer to the bitmap.
 * @param low Low index of the range (inclusive).
 * @param high High index of the range (exclusive).
 */
static inline void bitmap_set_range(bitmap_t* map, uint64_t low, uint64_t high)
{
    if (low >= high || high > map->length)
    {
        return;
    }

    for (uint64_t i = low; i < high; i++)
    {
        bitmap_set(map, i);
    }
}

/**
 * @brief Find a clear region of specified length and alignment, and set it.
 *
 * @param map Pointer to the bitmap.
 * @param length Length of the region to find.
 * @param maxIdx Maximum index to search up to.
 * @param align Alignment of the region.
 * @return Starting index of the found region, or `map->length` if not found.
 */
static inline uint64_t bitmap_find_clear_region_and_set(bitmap_t* map, uint64_t length, uintptr_t maxIdx,
    uint64_t align)
{
    if (length == 0 || align == 0 || maxIdx > map->length)
    {
        return map->length;
    }

    for (uint64_t i = map->firstZeroIdx; i < maxIdx; i += align)
    {
        if (!bitmap_is_set(map, i))
        {
            uint64_t j = i + 1;
            for (; j < maxIdx; j++)
            {
                if (j - i == length)
                {
                    bitmap_set_range(map, i, j);
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

    return map->length;
}

/**
 * @brief Clear a bit in the bitmap.
 *
 * @param map Pointer to the bitmap.
 * @param index Index of the bit to clear.
 */
static inline void bitmap_clear(bitmap_t* map, uint64_t index)
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

/**
 * @brief Clear a range of bits in the bitmap.
 *
 * @param map Pointer to the bitmap.
 * @param low Low index of the range (inclusive).
 * @param high High index of the range (exclusive).
 */
static inline void bitmap_clear_range(bitmap_t* map, uint64_t low, uint64_t high)
{
    if (low >= high || high > map->length)
    {
        return;
    }

    for (uint64_t i = low; i < high; i++)
    {
        uint64_t qwordIdx = i / 64;
        uint64_t bitInQword = i % 64;
        map->buffer[qwordIdx] &= ~(1ULL << bitInQword);
    }
    map->firstZeroIdx = MIN(map->firstZeroIdx, low);
}

/**
 * @brief Sum the number of set bits in a range.
 *
 * @param map Pointer to the bitmap.
 * @param low Low index of the range (inclusive).
 * @param high High index of the range (exclusive).
 * @return Number of set bits in the range.
 */
static inline uint64_t bitmap_sum(bitmap_t* map, uint64_t low, uint64_t high)
{
    if (low >= high || high > map->length)
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

/**
 * @brief Find the first clear bit in the bitmap.
 *
 * @param map The bitmap.
 * @return Index of the first clear bit, or `map->length` if none found.
 */
static inline uint64_t bitmap_find_first_clear(bitmap_t* map)
{
    if (map->firstZeroIdx >= map->length)
    {
        return map->length;
    }

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

/**
 * @brief Find the first set bit in the bitmap.
 *
 * @param map The bitmap.
 * @param startIdx Index to start searching from.
 * @return Index of the first set bit, or `map->length` if none found.
 */
static inline uint64_t bitmap_find_first_set(bitmap_t* map, uint64_t startIdx)
{
    if (startIdx >= map->length)
    {
        return map->length;
    }

    uint64_t startQwordIdx = startIdx / 64;
    uint64_t startBitIdx = startIdx % 64;
    uint64_t endQwordIdx = BITMAP_BITS_TO_QWORDS(map->length);

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

/** @} */

#endif // _SYS_BITMAP_H
