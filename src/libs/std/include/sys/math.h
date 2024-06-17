#ifndef _SYS_MATH_H
#define _SYS_MATH_H 1

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define CLAMP(x, low, high) MIN((high), MAX((low), (x)))

#endif
