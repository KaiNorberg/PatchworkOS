#ifndef _SYS_MATH_H
#define _SYS_MATH_H 1

#include <stdint.h>

/**
 * @brief Helper Macros for math operations.
 * @ingroup libc
 * @defgroup libc_math Math Helpers
 *
 * The `sys/math.h` header provides common math macros for operations such as clamping, rounding, and linear
 * interpolation.
 *
 * @{
 */

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define CLAMP(x, low, high) MIN((high), MAX((low), (x)))

#define ROUND_UP(number, multiple) \
    ((((uint64_t)(number) + (uint64_t)(multiple) - 1) / (uint64_t)(multiple)) * (uint64_t)(multiple))
#define ROUND_DOWN(number, multiple) (((uint64_t)(number) / (uint64_t)(multiple)) * (uint64_t)(multiple))

#define LERP_INT(start, end, t, minT, maxT) (((start) * ((maxT) - (t))) + ((end) * ((t) - (minT)))) / ((maxT) - (minT))

#define IS_POW2(x) (((x) & ((x) - 1)) == 0)

uint64_t next_pow2(uint64_t n);

/**
 * @brief Counts the number of set bits in a 64-bit number.
 *
 * @see https://en.wikipedia.org/wiki/SWAR#Counting_bits_set
 *
 * @param x The number to check
 * @return The number of set bits.
 */
static inline uint64_t count_set_bits(uint64_t x)
{
    uint64_t x2 = (x & 0x5555555555555555) + ((x >> 1) & 0x5555555555555555);
    uint64_t x4 = (x2 & 0x3333333333333333) + ((x2 >> 2) & 0x3333333333333333);
    uint64_t x8 = (x4 + (x4 >> 4)) & 0x0f0f0f0f0f0f0f0f;
    uint64_t x16 = x8 + (x8 >> 8);
    uint64_t x32 = x16 + (x16 >> 16);
    uint64_t x64 = x32 + (x32 >> 32);
    return x64 & 0xff;
}

#endif

/** @} */
