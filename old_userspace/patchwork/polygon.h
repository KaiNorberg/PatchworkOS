#ifndef PATCHWORK_POLYGON
#define PATCHWORK_POLYGON 1

#include "cmd.h"
#include "font.h"
#include "pixel.h"
#include "rect.h"
#include "surface.h"

#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct image image_t;

/**
 * @brief Polygon.
 * @defgroup libpatchwork_polygon Polygon
 * @ingroup libpatchwork
 *
 * @{
 */

/**
 * @brief Rotate a polygon around a center point.
 *
 * @param points The points of the polygon.
 * @param pointCount The number of points in the polygon.
 * @param angle The angle to rotate by, in radians.
 * @param center The center point to rotate around.
 */
void polygon_rotate(point_t* points, uint64_t pointCount, double angle, point_t center);

/**
 * @brief Check if a point is inside a polygon.
 *
 * We use doubles for the point coordinates instead of just `point_t` to allow for sub-pixel points.
 *
 * @param px The x coordinate of the point.
 * @param py The y coordinate of the point.
 * @param points The points of the polygon.
 * @param pointCount The number of points in the polygon.
 * @return `true` if the point is inside the polygon, `false` otherwise.
 */
bool polygon_contains(double px, double py, const point_t* points, uint64_t pointCount);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
