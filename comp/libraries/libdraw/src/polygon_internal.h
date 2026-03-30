#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct polygon_edge
{
    float top;
    float bottom;
    float start;
    float invSlope;
    int8_t dir;
    float x;
} polygon_edge_t;

typedef struct polygon
{
    float top;
    float bottom;
    size_t edgeCount;
    polygon_edge_t** activeEdges;
    polygon_edge_t edges[];
} polygon_t;
