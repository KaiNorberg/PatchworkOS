#ifndef _SYS_MATH_H
#define _SYS_MATH_H 1

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define CLAMP(x, low, high) MIN((high), MAX((low), (x)))

#define ROUND_UP(number, multiple) \
    ((((uint64_t)(number) + (uint64_t)(multiple) - 1) / (uint64_t)(multiple)) * (uint64_t)(multiple))
#define ROUND_DOWN(number, multiple) (((uint64_t)(number) / (uint64_t)(multiple)) * (uint64_t)(multiple))

#endif
