#ifndef _AUX_RECT_T_H
#define _AUX_RECT_T_H 1

#include <stdint.h>

typedef struct rect
{
    uint64_t left;
    uint64_t top;
    uint64_t right;
    uint64_t bottom;
} rect_t;

#define RECT_INIT(rect, left, top, right, bottom) \
    ({ \
        *rect = (rect_t){ \
            left, \
            top, \
            right, \
            bottom, \
        }; \
    })

#define RECT_INIT_DIM(rect, x, y, width, height) \
    ({ \
        *rect = (rect_t){ \
            (x), \
            (y), \
            (x) + (width), \
            (y) + (height), \
        }; \
    })

#define RECT_WIDTH(rect) ((rect)->right - (rect)->left)
#define RECT_HEIGHT(rect) ((rect)->bottom - (rect)->top)
#define RECT_AREA(rect) (RECT_WIDTH(rect) * RECT_HEIGHT(rect))

#endif
