#pragma once

#include <libgfx/rect.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Polygon helpers
 * @defgroup comp_libgfx_poly Polygons
 * @ingroup comp_libgfx
 *
 * @{
 */

/**
 * @brief Parsed polygon structure for drawing.
 * @struct gfx_poly_t
 */
typedef struct gfx_poly gfx_poly_t;

/**
 * @brief Create a new polygon structure from a set of vertices.
 *
 * @param vertices An array of x, y coordinates.
 * @param count The number of vertices.
 * @return A pointer to the newly created polygon structure.
 */
gfx_poly_t* gfx_poly_new(const float* vertices, size_t count);

/**
 * @brief Free a polygon structure.
 *
 * @param polygon The polygon to free.
 */
void gfx_poly_free(gfx_poly_t* polygon);

/** @} */