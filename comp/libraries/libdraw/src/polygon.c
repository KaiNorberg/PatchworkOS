#include <libdraw/polygon.h>
#include <libdraw/vertices.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <float.h>
#include <libstd/math.h>
#include <stdlib.h>

static inline int polygon_edge_compare(const void* a, const void* b)
{
    polygon_edge_t* edgeA = (polygon_edge_t*)a;
    polygon_edge_t* edgeB = (polygon_edge_t*)b;
    return (edgeA->top < edgeB->top) ? -1 : (edgeA->top > edgeB->top);
}

polygon_t* polygon_new(const float* vertices, size_t count)
{
    if (vertices == NULL || count == 0)
    {
        return NULL;
    }

    polygon_t* polygon = malloc(sizeof(polygon_t) + count * sizeof(polygon_edge_t) + count * sizeof(polygon_edge_t*));
    if (polygon == NULL)
    {
        return NULL;
    }

    polygon->top = FLT_MAX;
    polygon->bottom = -FLT_MAX;

    for (uint64_t i = 0; i < count; i++)
    {
        polygon->top = MIN(polygon->top, VERTICES_GET_Y(vertices, i));
        polygon->bottom = MAX(polygon->bottom, VERTICES_GET_Y(vertices, i));
    }

    polygon->edgeCount = 0;
    for (uint64_t i = 0; i < count; i++)
    {
        float x1 = VERTICES_GET_X(vertices, i);
        float y1 = VERTICES_GET_Y(vertices, i);
        float x2 = VERTICES_GET_X(vertices, (i + 1) % count);
        float y2 = VERTICES_GET_Y(vertices, (i + 1) % count);

        if (y1 == y2)
        {
            continue;
        }

        polygon_edge_t* edge = &polygon->edges[polygon->edgeCount++];
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

    qsort(polygon->edges, polygon->edgeCount, sizeof(polygon_edge_t), polygon_edge_compare);

    polygon->activeEdges = (polygon_edge_t**)((uintptr_t)polygon->edges + (polygon->edgeCount * sizeof(polygon_edge_t)));

    return polygon;
}

void polygon_free(polygon_t* polygon)
{
    free(polygon);
}
