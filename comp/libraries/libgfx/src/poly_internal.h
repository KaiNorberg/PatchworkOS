#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct gfx_poly_edge
{
    float top;
    float bottom;
    float start;
    float invSlope;
    int8_t dir;
    float x;
} gfx_poly_edge_t;

typedef struct gfx_poly
{
    float top;
    float bottom;
    size_t edgeCount;
    gfx_poly_edge_t** activeEdges;
    gfx_poly_edge_t edges[];
} gfx_poly_t;
