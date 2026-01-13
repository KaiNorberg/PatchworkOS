#ifndef _SYS_MATH_H
#define _SYS_MATH_H 1

#include <stdint.h>

/**
 * @brief Common math macros.
 * @ingroup libstd
 * @defgroup libstd_sys_math Common math macros
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

#endif

/** @} */
