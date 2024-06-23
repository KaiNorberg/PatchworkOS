#ifndef _AUX_RECT_T_H
#define _AUX_RECT_T_H 1

#include <stdint.h>

typedef struct rect
{
    int64_t left;
    int64_t top;
    int64_t right;
    int64_t bottom;
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

#define RECT_FIT(rect, parent) \
    ({ \
        (rect)->left = CLAMP((rect)->left, (parent)->left, (parent)->right); \
        (rect)->top = CLAMP((rect)->top, (parent)->top, (parent)->bottom); \
        (rect)->right = CLAMP((rect)->right, (parent)->left, (parent)->right); \
        (rect)->bottom = CLAMP((rect)->bottom, (parent)->top, (parent)->bottom); \
    })

#define RECT_CONTAINS(parent, child) \
    ((child)->left >= (parent)->left && (child)->right <= (parent)->right && (child)->top >= (parent)->top && \
        (child)->bottom <= (parent)->bottom)

#define RECT_WIDTH(rect) ((rect)->right - (rect)->left)
#define RECT_HEIGHT(rect) ((rect)->bottom - (rect)->top)
#define RECT_AREA(rect) (RECT_WIDTH(rect) * RECT_HEIGHT(rect))

#endif
