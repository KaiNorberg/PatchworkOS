#pragma once

#include <libc/math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Rectangle definitions
 * @defgroup comp_libgfx_rect Rectangle
 * @ingroup comp_libgfx
 *
 * @{
 */

/**
 * @brief Rectangle structure.
 * @struct gfx_rect_t
 */
typedef struct gfx_rect
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} gfx_rect_t;

/**
 * @brief Empty rectangle constant.
 * @def GFX_RECT_EMPTY
 */
#define GFX_RECT_EMPTY (gfx_rect_t){0, 0, 0, 0}

/**
 * @brief Create a rectangle from a top-left corner and extents.
 *
 * @param _l Left coordinate.
 * @param _t Top coordinate.
 * @param _w Width of the rectangle.
 * @param _h Height of the rectangle.
 */
#define GFX_RECT(_l, _t, _w, _h) \
    (gfx_rect_t) \
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
#define GFX_RECT_FROM_CORNERS(_l, _t, _r, _b) \
    (gfx_rect_t) \
    { \
        (_l), (_t), (_r), (_b) \
    }

/**
 * @brief Create a rectangle from a center point and extents.
 *
 * @param _x Center x coordinate.
 * @param _y Center y coordinate.
 * @param _w Width of the rectangle.
 * @param _h Height of the rectangle.
 */
#define GFX_RECT_FROM_CENTER(_x, _y, _w, _h) \
    (gfx_rect_t) \
    { \
        (_x) - ((_w) / 2), (_y) - ((_h) / 2), (_x) + ((_w) / 2), (_y) + ((_h) / 2) \
    }

/**
 * @brief Get the width of a rectangle.
 *
 * @param rect The rectangle.
 */
#define GFX_RECT_WIDTH(rect) ((rect).right - (rect).left)

/**
 * @brief Get the height of a rectangle.
 *
 * @param rect The rectangle.
 */
#define GFX_RECT_HEIGHT(_rect) ((_rect).bottom - (_rect).top)

/**
 * @brief Get the area of a rectangle.
 *
 * @param _rect The rectangle.
 */
#define GFX_RECT_AREA(_rect) (GFX_RECT_WIDTH(_rect) * GFX_RECT_HEIGHT(_rect))

/**
 * @brief Get the center of a rectangle.
 *
 * @param _rect The rectangle.
 */
#define GFX_RECT_CENTER(_rect) \
    (gfx_rect_t){(_rect).left + (GFX_RECT_WIDTH(_rect) / 2), (_rect).top + (GFX_RECT_HEIGHT(_rect) / 2), 0, 0}

/**
 * @brief Check if a rectangle is valid (has positive area).
 *
 * @param _rect The rectangle.
 */
#define GFX_RECT_VALID(_rect) ((_rect).right > (_rect).left && (_rect).bottom > (_rect).top)

/**
 * @brief Check if two rectangles intersect (including edges)
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define GFX_RECT_OVERLAP(_a, _b) \
    ((_a).left <= (_b).right && (_a).right >= (_b).left && (_a).top <= (_b).bottom && (_a).bottom >= (_b).top)

/**
 * @brief Check if two rectangles intersect (excluding edges).
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define GFX_RECT_OVERLAP_STRICT(_a, _b) \
    ((_a).left < (_b).right && (_a).right > (_b).left && (_a).top < (_b).bottom && (_a).bottom > (_b).top)

/**
 * @brief Get the intersection of two rectangles.
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define GFX_RECT_INTERSECTION(_a, _b) \
    (gfx_rect_t) \
    { \
        MAX((_a).left, (_b).left), MAX((_a).top, (_b).top), MIN((_a).right, (_b).right), MIN((_a).bottom, (_b).bottom) \
    }

/**
 * @brief Get the union of two rectangles.
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define GFX_RECT_UNION(_a, _b) \
    (gfx_rect_t) \
    { \
        MIN((_a).left, (_b).left), MIN((_a).top, (_b).top), MAX((_a).right, (_b).right), MAX((_a).bottom, (_b).bottom) \
    }

/**
 * @brief Check if a point is inside a rectangle.
 *
 * @param _rect The rectangle.
 * @param _x The x coordinate of the point.
 * @param _y The y coordinate of the point.
 */
#define GFX_RECT_CONTAINS(_rect, _x, _y) \
    ((_x) >= (_rect).left && (_x) < (_rect).right && (_y) >= (_rect).top && (_y) < (_rect).bottom)

/**
 * @brief Check if two rectangles are equal.
 *
 * @param _a The first rectangle.
 * @param _b The second rectangle.
 */
#define GFX_RECT_EQUALS(_a, _b) \
    ((_a).left == (_b).left && (_a).top == (_b).top && (_a).right == (_b).right && (_a).bottom == (_b).bottom)

/**
 * @brief Offset a rectangle by a given amount.
 *
 * @param _rect The rectangle.
 * @param _x The x offset.
 * @param _y The y offset.
 */
#define GFX_RECT_OFFSET(_rect, _x, _y) \
    (gfx_rect_t) \
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
#define GFX_RECT_EXPAND(_rect, _x, _y) \
    (gfx_rect_t) \
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
#define GFX_RECT_SHRINK(_rect, _x, _y) \
    (gfx_rect_t) \
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
#define GFX_RECT_MOVE(_rect, _x, _y) \
    (gfx_rect_t) \
    { \
        (_x), (_y), (_x) + GFX_RECT_WIDTH(_rect), (_y) + GFX_RECT_HEIGHT(_rect) \
    }

/**
 * @brief Structure to hold the difference between two rectangles.
 * @struct gfx_rect_difference_t
 */
typedef struct gfx_rect_difference
{
    gfx_rect_t rects[4];
    uint32_t count;
} gfx_rect_difference_t;

/**
 * @brief Calculate the difference between two rectangles.
 *
 * This function calculates the parts of rectangle A that are not covered by rectangle B.
 * The result is stored as up to 4 smaller rectangles in the output structure.
 *
 * @param out Pointer to the structure to store the resulting rectangles.
 * @param a The source rectangle.
 * @param b The rectangle to subtract from A.
 */
static inline void gfx_rect_difference(gfx_rect_difference_t* out, gfx_rect_t a, gfx_rect_t b)
{
    out->count = 0;

    if (!GFX_RECT_OVERLAP(a, b))
    {
        out->rects[out->count++] = a;
        return;
    }

    gfx_rect_t intersect = GFX_RECT_INTERSECTION(a, b);

    if (intersect.top > a.top)
    {
        out->rects[out->count++] = GFX_RECT_FROM_CORNERS(a.left, a.top, a.right, intersect.top);
    }

    if (intersect.bottom < a.bottom)
    {
        out->rects[out->count++] = GFX_RECT_FROM_CORNERS(a.left, intersect.bottom, a.right, a.bottom);
    }

    if (intersect.left > a.left)
    {
        out->rects[out->count++] = GFX_RECT_FROM_CORNERS(a.left, intersect.top, intersect.left, intersect.bottom);
    }

    if (intersect.right < a.right)
    {
        out->rects[out->count++] = GFX_RECT_FROM_CORNERS(intersect.right, intersect.top, a.right, intersect.bottom);
    }
}

/**
 * @brief Clip two rectangles simultaneously against their respective bounding boxes.
 *
 * @param dstBounds The bounding box for the destination rectangle.
 * @param srcBounds The bounding box for the source rectangle.
 * @param dst The destination rectangle to clip.
 * @param src The source rectangle to clip.
 * @return `true` if the resulting rectangles have a positive area, `false` otherwise.
 */
static inline bool gfx_rect_clip_dual(gfx_rect_t dstBounds, gfx_rect_t srcBounds, gfx_rect_t* dst, gfx_rect_t* src)
{
    int32_t width = MIN(GFX_RECT_WIDTH(*dst), GFX_RECT_WIDTH(*src));
    int32_t height = MIN(GFX_RECT_HEIGHT(*dst), GFX_RECT_HEIGHT(*src));

    if (dst->left < dstBounds.left)
    {
        int32_t diff = dstBounds.left - dst->left;
        width -= diff;
        src->left += diff;
        dst->left = dstBounds.left;
    }
    if (dst->top < dstBounds.top)
    {
        int32_t diff = dstBounds.top - dst->top;
        height -= diff;
        src->top += diff;
        dst->top = dstBounds.top;
    }
    if (dst->left + width > dstBounds.right)
    {
        width = dstBounds.right - dst->left;
    }
    if (dst->top + height > dstBounds.bottom)
    {
        height = dstBounds.bottom - dst->top;
    }

    if (src->left < srcBounds.left)
    {
        int32_t diff = srcBounds.left - src->left;
        width -= diff;
        dst->left += diff;
        src->left = srcBounds.left;
    }
    if (src->top < srcBounds.top)
    {
        int32_t diff = srcBounds.top - src->top;
        height -= diff;
        dst->top += diff;
        src->top = srcBounds.top;
    }
    if (src->left + width > srcBounds.right)
    {
        width = srcBounds.right - src->left;
    }
    if (src->top + height > srcBounds.bottom)
    {
        height = srcBounds.bottom - src->top;
    }

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    dst->right = dst->left + width;
    dst->bottom = dst->top + height;
    src->right = src->left + width;
    src->bottom = src->top + height;

    return true;
}

/** @} */