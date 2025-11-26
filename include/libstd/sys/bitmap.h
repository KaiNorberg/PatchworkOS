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
 * @brief Define and create a bitmap and its buffer.
 *
 * @param name Name of the bitmap.
 * @param bits Length of the bitmap in bits.
 */
#define BITMAP_CREATE(name, bits) \
    uint64_t name##Buffer[BITMAP_BITS_TO_QWORDS(bits)]; \
    bitmap_t name = {.firstZeroIdx = 0, .length = (bits), .buffer = name##Buffer}

/**
 * @brief Define a bitmap and its buffer.
 *
 * Will not initialize the bitmap, use `BITMAP_DEFINE_INIT` to initialize it.
 *
 * Intended to be used for struct members.
 *
 * @param name Name of the bitmap.
 * @param bits Length of the bitmap in bits.
 */
#define BITMAP_DEFINE(name, bits) \
    uint64_t name##Buffer[BITMAP_BITS_TO_QWORDS(bits)]; \
    bitmap_t name

/**
 * @brief Initialize a bitmap defined with `BITMAP_DEFINE`.
 *
 * @param name Name of the bitmap.
 * @param bits Length of the bitmap in bits.
 */
#define BITMAP_DEFINE_INIT(name, bits) bitmap_init(&(name), name##Buffer, bits);

/**
 * @brief Initialize a bitmap.
 *
 * @param map The bitmap.
 * @param buffer Pointer to the buffer, must be a multiple of 8 bytes.
 * @param length Length of the bitmap in bits.
 */
void bitmap_init(bitmap_t* map, void* buffer, uint64_t length);

/**
 * @brief Check if a bit is set in the bitmap.
 *
 * @param map The bitmap.
 * @param idx Index of the bit to check.
 * @return true if the bit is set, false otherwise.
 */
bool bitmap_is_set(bitmap_t* map, uint64_t idx);

/**
 * @brief Set a bit in the bitmap.
 *
 * @param map Pointer to the bitmap.
 * @param index Index of the bit to set.
 */
void bitmap_set(bitmap_t* map, uint64_t index);

/**
 * @brief Set a range of bits in the bitmap.
 *
 * @param map Pointer to the bitmap.
 * @param low Low index of the range (inclusive).
 * @param high High index of the range (exclusive).
 */
void bitmap_set_range(bitmap_t* map, uint64_t low, uint64_t high);

/**
 * @brief Clear a bit in the bitmap.
 *
 * @param map Pointer to the bitmap.
 * @param index Index of the bit to clear.
 */
void bitmap_clear(bitmap_t* map, uint64_t index);

/**
 * @brief Clear a range of bits in the bitmap.
 *
 * @param map Pointer to the bitmap.
 * @param low Low index of the range (inclusive).
 * @param high High index of the range (exclusive).
 */
void bitmap_clear_range(bitmap_t* map, uint64_t low, uint64_t high);

/**
 * @brief Find the first clear bit in the bitmap.
 *
 * @param map The bitmap.
 * @param startIdx Index to start searching from.
 * @param endIdx Index to stop searching at (exclusive).
 * @return Index of the first clear bit, or `map->length` if none found.
 */
uint64_t bitmap_find_first_clear(bitmap_t* map, uint64_t startIdx, uint64_t endIdx);

/**
 * @brief Find the first set bit in the bitmap.
 *
 * @param map The bitmap.
 * @param startIdx Index to start searching from.
 * @param endIdx Index to stop searching at (exclusive).
 * @return Index of the first set bit, or `map->length` if none found.
 */
uint64_t bitmap_find_first_set(bitmap_t* map, uint64_t startIdx, uint64_t endIdx);

/**
 * @brief Find a clear region of specified length and alignment, and set it.
 *
 * @param map Pointer to the bitmap.
 * @param minIdx Minimum index to start searching from.
 * @param maxIdx Maximum index to search up to.
 * @param length Length of the region to find.
 * @param alignment Alignment of the region.
 * @return Starting index of the found region, or `map->length` if not found.
 */
uint64_t bitmap_find_clear_region_and_set(bitmap_t* map, uint64_t minIdx, uintptr_t maxIdx, uint64_t length,
    uint64_t alignment);

/** @} */

#endif // _SYS_BITMAP_H
