#ifndef PATCHWORK_POINT_H
#define PATCHWORK_POINT_H 1

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct point
{
    int64_t x;
    int64_t y;
} point_t;

#if defined(__cplusplus)
}
#endif

#endif
