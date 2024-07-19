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

typedef struct
{
    rect_t rects[4];
    uint8_t count;
} rect_subtract_t;

#define RECT_INIT(left, top, right, bottom) \
    (rect_t) \
    { \
        left, top, right, bottom, \
    }

#define RECT_INIT_DIM(x, y, width, height) \
    (rect_t) \
    { \
        (x), (y), (x) + (width), (y) + (height), \
    }

#define RECT_WIDTH(rect) ((rect)->right - (rect)->left)
#define RECT_HEIGHT(rect) ((rect)->bottom - (rect)->top)
#define RECT_AREA(rect) (RECT_WIDTH(rect) * RECT_HEIGHT(rect))

#define RECT_CONTAINS(rect, other) \
    ((other)->left >= (rect)->left && (other)->right <= (rect)->right && (other)->top >= (rect)->top && \
        (other)->bottom <= (rect)->bottom)

#define RECT_CONTAINS_POINT(rect, x, y) ((x) >= (rect)->left && (x) < (rect)->right && (y) >= (rect)->top && (y) < (rect)->bottom)

#define RECT_OVERLAP(rect, other) \
    (!((rect)->right <= (other)->left || (rect)->left >= (other)->right || (rect)->bottom <= (other)->top || \
        (rect)->top >= (other)->bottom))

#define RECT_FIT(rect, parent) \
    ({ \
        (rect)->left = CLAMP((rect)->left, (parent)->left, (parent)->right); \
        (rect)->top = CLAMP((rect)->top, (parent)->top, (parent)->bottom); \
        (rect)->right = CLAMP((rect)->right, (parent)->left, (parent)->right); \
        (rect)->bottom = CLAMP((rect)->bottom, (parent)->top, (parent)->bottom); \
    })

#define RECT_SHRINK(rect, margin) \
    ({ \
        (rect)->left += margin; \
        (rect)->top += margin; \
        (rect)->right -= margin; \
        (rect)->bottom -= margin; \
    })

#define RECT_EXPAND(rect, margin) \
    ({ \
        (rect)->left -= margin; \
        (rect)->top -= margin; \
        (rect)->right += margin; \
        (rect)->bottom += margin; \
    })

#define RECT_SUBTRACT(result, rect, other) \
    ({ \
        rect_subtract_t res = {.count = 0}; \
        if (!RECT_OVERLAP(rect, other)) \
        { \
            res.rects[0] = *(rect); \
            res.count = 1; \
        } \
        else \
        { \
            if ((other)->top > (rect)->top) \
            { \
                res.rects[res.count++] = (rect_t){(rect)->left, (rect)->top, (rect)->right, (other)->top}; \
            } \
            if ((other)->bottom < (rect)->bottom) \
            { \
                res.rects[res.count++] = (rect_t){(rect)->left, (other)->bottom, (rect)->right, (rect)->bottom}; \
            } \
            if ((other)->left > (rect)->left) \
            { \
                res.rects[res.count++] = (rect_t){(rect)->left, (other)->top, (other)->left, (other)->bottom}; \
            } \
            if ((other)->right < (rect)->right) \
            { \
                res.rects[res.count++] = (rect_t){(other)->right, (other)->top, (rect)->right, (other)->bottom}; \
            } \
        } \
        *(result) = res; \
    })

#endif
