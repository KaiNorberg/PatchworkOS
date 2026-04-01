#pragma once

#include <libgfx/rect.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Region definitions
 * @defgroup comp_libgfx_region Region
 * @ingroup comp_libgfx
 *
 * A region is a collection of rectangles that represent a non-rectangular area.
 *
 * Regions are primarily used for tracking dirty/invalid areas that need to be redrawn.
 *
 * @{
 */

#define GFX_REGION_MAX_RECTS 128 ///< The maximum number of rectangles in a region.

/**
 * @brief Region structure.
 * @struct gfx_region_t
 */
typedef struct
{
    gfx_rect_t rects[GFX_REGION_MAX_RECTS];
    uint64_t count;
} gfx_region_t;

/**
 * @brief Create a new region.
 */
#define GFX_REGION() (gfx_region_t){.count = 0}

/**
 * @brief Add a rectangle to a region, potentially merging it with existing rectangles.
 *
 * @param region The region to add the rectangle to.
 * @param rect The rectangle to add.
 */
static inline void gfx_region_add(gfx_region_t* region, gfx_rect_t rect)
{
    if (GFX_RECT_AREA(rect) == 0)
    {
        return;
    }

    gfx_rect_t newRect = rect;
    for (uint64_t i = 0; i < region->count; i++)
    {
        if (GFX_RECT_OVERLAP_STRICT(region->rects[i], rect))
        {
            newRect = GFX_RECT_UNION(newRect, region->rects[i]);
            region->rects[i] = region->rects[region->count - 1];
            region->count--;
            i--;
        }
    }

    if (region->count < GFX_REGION_MAX_RECTS)
    {
        region->rects[region->count] = newRect;
        region->count++;
    }
    else
    {
        gfx_rect_t mergedRect = region->rects[0];
        for (uint64_t i = 1; i < region->count; i++)
        {
            mergedRect = GFX_RECT_UNION(mergedRect, region->rects[i]);
        }
        mergedRect = GFX_RECT_UNION(mergedRect, newRect);
        region->rects[0] = mergedRect;
        region->count = 1;
    }
}

/**
 * @brief Subtract a rectangle from a region.
 *
 * @param region The region to subtract from.
 * @param subRect The rectangle to subtract.
 */
static inline void gfx_region_subtract(gfx_region_t* region, gfx_rect_t subRect)
{
    gfx_region_t result = GFX_REGION();
    for (uint64_t i = 0; i < region->count; i++)
    {
        gfx_rect_difference_t diff;
        gfx_rect_difference(&diff, region->rects[i], subRect);
        for (uint32_t j = 0; j < diff.count; j++)
        {
            gfx_region_add(&result, diff.rects[j]);
        }
    }

    *region = result;
}