#ifndef PATCHWORK_RECT_H
#define PATCHWORK_RECT_H 1

#include <stdint.h>
#include <sys/math.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct rect
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
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

#define RECT_HAS_NEGATIVE_DIMS(rect) (RECT_WIDTH(rect) < 0 || RECT_HEIGHT(rect) < 0)

#define RECT_EXPAND_TO_CONTAIN(rect, other) \
    ({ \
        (rect)->left = MIN((rect)->left, (other)->left); \
        (rect)->top = MIN((rect)->top, (other)->top); \
        (rect)->right = MAX((rect)->right, (other)->right); \
        (rect)->bottom = MAX((rect)->bottom, (other)->bottom); \
    })

#define RECT_EQUAL(rect, other) \
    ((other)->left == (rect)->left && (other)->right == (rect)->right && (other)->top == (rect)->top && \
        (other)->bottom == (rect)->bottom)

#define RECT_CONTAINS(rect, other) \
    ((rect)->left <= (rect)->right && (rect)->top <= (rect)->bottom && (other)->left <= (other)->right && \
        (other)->top <= (other)->bottom && (other)->left >= (rect)->left && (other)->right <= (rect)->right && \
        (other)->top >= (rect)->top && (other)->bottom <= (rect)->bottom)

#define RECT_CONTAINS_POINT(rect, point) \
    ((point)->x >= (rect)->left && (point)->x < (rect)->right && (point)->y >= (rect)->top && \
        (point)->y < (rect)->bottom)

#define RECT_OVERLAP(rect, other) \
    (!((rect)->right <= (other)->left || (rect)->left >= (other)->right || (rect)->bottom <= (other)->top || \
        (rect)->top >= (other)->bottom))

#define RECT_OVERLAP_STRICT(rect, other) \
    (!((rect)->right < (other)->left || (rect)->left > (other)->right || (rect)->bottom < (other)->top || \
        (rect)->top > (other)->bottom))

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

#if defined(__cplusplus)
}
#endif

#endif
