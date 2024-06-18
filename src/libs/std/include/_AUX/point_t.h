#ifndef _AUX_POINT_T_H
#define _AUX_POINT_T_H 1

#include <stdint.h>

typedef struct point
{
    uint64_t x;
    uint64_t y;
} point_t;

#define POINT_INIT(point, x, y) \
    ({ \
        *point = (point_t){ \
            x, \
            y, \
        }; \
    })

#endif
