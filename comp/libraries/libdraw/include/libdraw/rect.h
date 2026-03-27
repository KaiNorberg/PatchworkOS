#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Rectangle definitions
 * @defgroup comp_libdraw_rect Rectangle
 * @ingroup comp_libdraw
 *
 * @{
 */

/**
 * @brief Rectangle structure.
 * @struct rect_t
 */
typedef struct
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} rect_t;

/**
 * @brief Empty rectangle constant.
 * @def RECT_EMPTY
 */
#define RECT_EMPTY (rect_t){0, 0, 0, 0}

/**
 * @brief Create a rectangle from a top-left corner and extents.
 *
 * @param _l Left coordinate.
 * @param _t Top coordinate.
 * @param _w Width of the rectangle.
 * @param _h Height of the rectangle.
 */
#define RECT(_l, _t, _w, _h) \
    (rect_t) \
    { \
        (_l), (_t), (_l) + (_w), (_t) + (_h) \
    }

/**
 * @brief Create a rectangle from corner coordinates.
 *
 * @param _l Left coordinate.
 * @param _t Top coordinate.
 * @param _r Right coordinate.
 * @param _b Bottom coordinate.
 */
#define RECT_FROM_CORNERS(_l, _t, _r, _b) \
    (rect_t) \
    { \
        (_l), (_t), (_r), (_b) \
    }

/**
 * @brief Get the width of a rectangle.
 *
 * @param rect The rectangle.
 */
#define RECT_WIDTH(rect) ((rect).right - (rect).left)

/**
 * @brief Get the height of a rectangle.
 *
 * @param rect The rectangle.
 */
#define RECT_HEIGHT(_rect) ((_rect).bottom - (_rect).top)

/**
 * @brief Get the area of a rectangle.
 *
 * @param _rect The rectangle.
 */
#define RECT_AREA(_rect) (RECT_WIDTH(_rect) * RECT_HEIGHT(_rect))

/**
 * @brief Get the center of a rectangle.
 *
 * @param _rect The rectangle.
 */
#define RECT_CENTER(_rect) \
    (rect_t){(_rect).left + (RECT_WIDTH(_rect) / 2), (_rect).top + (RECT_HEIGHT(_rect) / 2), 0, 0}

/**
 * @brief Check if a rectangle is valid (has positive area).
 *
 * @param _rect The rectangle.
 */
#define RECT_VALID(_rect) ((_rect).right > (_rect).left && (_rect).bottom > (_rect).top)

/**
 * @brief Check if two rectangles intersect.
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define RECT_OVERLAP(_a, _b) \
    ((_a).left < (_b).right && (_a).right > (_b).left && (_a).top < (_b).bottom && (_a).bottom > (_b).top)

/**
 * @brief Check if two rectangles intersect (including edges).
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define RECT_OVERLAP_STRICT(_a, _b) \
    ((_a).left <= (_b).right && (_a).right >= (_b).left && (_a).top <= (_b).bottom && (_a).bottom >= (_b).top)

/**
 * @brief Get the intersection of two rectangles.
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define RECT_INTERSECTION(_a, _b) \
    (rect_t) \
    { \
        (_a).left > (_b).left ? (_a).left : (_b).left, (_a).top > (_b).top ? (_a).top : (_b).top, \
            (_a).right < (_b).right ? (_a).right : (_b).right, (_a).bottom < (_b).bottom ? (_a).bottom : (_b).bottom \
    }

/**
 * @brief Get the union of two rectangles.
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define RECT_UNION(_a, _b) \
    (rect_t) \
    { \
        (_a).left<(_b).left ? (_a).left : (_b).left, \
                (_a).top<(_b).top ? (_a).top : (_b).top, (_a).right>(_b).right ? (_a).right : (_b).right, \
                (_a).bottom>(_b) \
                .bottom \
            ? (_a).bottom \
            : (_b).bottom \
    }

/**
 * @brief Check if a point is inside a rectangle.
 *
 * @param _rect The rectangle.
 * @param _x The x coordinate of the point.
 * @param _y The y coordinate of the point.
 */
#define RECT_CONTAINS(_rect, _x, _y) \
    ((_x) >= (_rect).left && (_x) < (_rect).right && (_y) >= (_rect).top && (_y) < (_rect).bottom)

/**
 * @brief Check if two rectangles are equal.
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define RECT_EQUALS(_a, _b) \
    ((_a).left == (_b).left && (_a).top == (_b).top && (_a).right == (_b).right && (_a).bottom == (_b).bottom)

/**
 * @brief Offset a rectangle by a given amount.
 *
 * @param _rect The rectangle.
 * @param _x The x offset.
 * @param _y The y offset.
 */
#define RECT_OFFSET(_rect, _x, _y) \
    (rect_t) \
    { \
        (_rect).left + (_x), (_rect).top + (_y), (_rect).right + (_x), (_rect).bottom + (_y) \
    }

/**
 * @brief Expand a rectangle by a given amount.
 *
 * @param _rect The rectangle.
 * @param _x The amount to expand the rectangle by in the x direction.
 * @param _y The amount to expand the rectangle by in the y direction.
 */
#define RECT_EXPAND(_rect, _x, _y) \
    (rect_t) \
    { \
        (_rect).left - (_x), (_rect).top - (_y), (_rect).right + (_x), (_rect).bottom + (_y) \
    }

/**
 * @brief Shrink a rectangle by a given amount.
 *
 * @param _rect The rectangle.
 * @param _x The amount to shrink the rectangle by in the x direction.
 * @param _y The amount to shrink the rectangle by in the y direction.
 */
#define RECT_SHRINK(_rect, _x, _y) \
    (rect_t) \
    { \
        (_rect).left + (_x), (_rect).top + (_y), (_rect).right - (_x), (_rect).bottom - (_y) \
    }

/**
 * @brief Move a rectangle to a new position.
 *
 * @param _rect The rectangle.
 * @param _x The new x coordinate.
 * @param _y The new y coordinate.
 */
#define RECT_MOVE(_rect, _x, _y) \
    (rect_t) \
    { \
        (_x), (_y), (_x) + RECT_WIDTH(_rect), (_y) + RECT_HEIGHT(_rect) \
    }

/** @} */