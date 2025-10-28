#pragma once

#include <libpatchwork/rect.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_REGION_RECTS 128

typedef struct
{
    rect_t rects[MAX_REGION_RECTS];
    uint64_t count;
} region_t;

#define REGION_CREATE {.count = 0}

static inline void region_init(region_t* region)
{
    region->count = 0;
}

static inline void region_clear(region_t* region)
{
    region->count = 0;
}

static inline bool region_is_empty(const region_t* region)
{
    return region->count == 0;
}

static inline void region_add(region_t* region, const rect_t* rect)
{
    if (RECT_AREA(rect) == 0)
    {
        return;
    }

    rect_t newRect = *rect;
    for (uint64_t i = 0; i < region->count; i++)
    {
        if (RECT_OVERLAP(&region->rects[i], rect))
        {
            RECT_EXPAND_TO_CONTAIN(&newRect, &region->rects[i]);
            region->rects[i] = region->rects[region->count - 1];
            region->count--;
            i--;
        }
    }

    if (region->count < MAX_REGION_RECTS)
    {
        region->rects[region->count] = newRect;
        region->count++;
    }
    else
    {
        rect_t mergedRect = region->rects[0];
        for (uint64_t i = 1; i < region->count; i++)
        {
            RECT_EXPAND_TO_CONTAIN(&mergedRect, &region->rects[i]);
        }
        RECT_EXPAND_TO_CONTAIN(&mergedRect, &newRect);
        region->rects[0] = mergedRect;
        region->count = 1;
    }
}

static inline void region_subtract(region_t* region, const rect_t* subRect)
{
    region_t result = REGION_CREATE;

    for (uint64_t i = 0; i < region->count; i++)
    {
        rect_subtract_t subRects;
        RECT_SUBTRACT(&subRects, &region->rects[i], subRect);
        for (uint64_t j = 0; j < subRects.count; j++)
        {
            region_add(&result, &subRects.rects[j]);
        }
    }

    *region = result;
}

static inline void region_intersect(region_t* region, region_t* out, const rect_t* clipRect)
{
    region_clear(out);

    for (uint64_t i = 0; i < region->count; i++)
    {
        rect_t intersect;
        RECT_INTERSECT(&intersect, &region->rects[i], clipRect);
        if (RECT_AREA(&intersect) > 0)
        {
            region_add(out, &intersect);
        }
    }
}
