#pragma once

#include <libdraw/rect.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Polygon helpers
 * @defgroup comp_libdraw_polygon Polygons
 * @ingroup comp_libdraw
 *
 * @{
 */

/**
 * @brief Edge structure for polygon drawing.
 * @struct polygon_edge_t
 */
typedef struct polygon_edge
{
    float top;
    float bottom;
    float start;
    float invSlope;
    int8_t dir;
    float x;
} polygon_edge_t;

/**
 * @brief Parsed polygon structure for drawing.
 * @struct polygon_t
 */
typedef struct polygon
{
    float top;
    float bottom;
    size_t edgeCount;
    polygon_edge_t** activeEdges;
    polygon_edge_t edges[];
} polygon_t;

/**
 * @brief Create a new polygon structure from a set of vertices.
 * 
 * @param vertices An array of x, y coordinates.
 * @param count The number of vertices.
 * @return polygon_t* 
 */
polygon_t* polygon_new(const float* vertices, size_t count);

/**
 * @brief Free a polygon structure.
 * 
 * @param polygon The polygon to free.
 */
void polygon_free(polygon_t* polygon);

/** @} */