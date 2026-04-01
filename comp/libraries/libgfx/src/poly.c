#include <float.h>
#include <libc/math.h>
#include <libgfx/poly.h>
#include <libgfx/verts.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "poly_internal.h"

static inline int gfx_poly_edge_compare(const void* a, const void* b)
{
    gfx_poly_edge_t* edgeA = (gfx_poly_edge_t*)a;
    gfx_poly_edge_t* edgeB = (gfx_poly_edge_t*)b;
    return (edgeA->top < edgeB->top) ? -1 : (edgeA->top > edgeB->top);
}

gfx_poly_t* gfx_poly_new(const float* vertices, size_t count)
{
    if (vertices == NULL || count == 0)
    {
        return NULL;
    }

    gfx_poly_t* polygon =
        malloc(sizeof(gfx_poly_t) + count * sizeof(gfx_poly_edge_t) + count * sizeof(gfx_poly_edge_t*));
    if (polygon == NULL)
    {
        return NULL;
    }

    polygon->top = FLT_MAX;
    polygon->bottom = -FLT_MAX;

    for (uint64_t i = 0; i < count; i++)
    {
        polygon->top = MIN(polygon->top, GFX_VERTS_GET_Y(vertices, i));
        polygon->bottom = MAX(polygon->bottom, GFX_VERTS_GET_Y(vertices, i));
    }

    polygon->edgeCount = 0;
    for (uint64_t i = 0; i < count; i++)
    {
        float x1 = GFX_VERTS_GET_X(vertices, i);
        float y1 = GFX_VERTS_GET_Y(vertices, i);
        float x2 = GFX_VERTS_GET_X(vertices, (i + 1) % count);
        float y2 = GFX_VERTS_GET_Y(vertices, (i + 1) % count);

        if (y1 == y2)
        {
            continue;
        }

        gfx_poly_edge_t* edge = &polygon->edges[polygon->edgeCount++];
        if (y1 < y2)
        {
            edge->top = y1;
            edge->bottom = y2;
            edge->start = x1;
            edge->invSlope = (x2 - x1) / (y2 - y1);
            edge->dir = 1;
        }
        else
        {
            edge->top = y2;
            edge->bottom = y1;
            edge->start = x2;
            edge->invSlope = (x1 - x2) / (y1 - y2);
            edge->dir = -1;
        }
    }

    qsort(polygon->edges, polygon->edgeCount, sizeof(gfx_poly_edge_t), gfx_poly_edge_compare);

    polygon->activeEdges =
        (gfx_poly_edge_t**)((uintptr_t)polygon->edges + (polygon->edgeCount * sizeof(gfx_poly_edge_t)));

    return polygon;
}

void gfx_poly_free(gfx_poly_t* polygon)
{
    free(polygon);
}
