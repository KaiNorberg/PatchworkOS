#pragma once

#include <libdraw/rect.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Vertices helpers
 * @defgroup comp_libdraw_vertices Vertices
 * @ingroup comp_libdraw
 *
 * @{
 */

/**
 * @brief Get the X coordinate from a vertices array.
 *
 * @param _vertices The vertices array.
 * @param _index The index of the point.
 */
#define VERTICES_GET_X(_vertices, _index) ((_vertices)[((_index) * 2) + 0])

/**
 * @brief Get the Y coordinate from a vertices array.
 *
 * @param _vertices The vertices array.
 * @param _index The index of the point.
 */
#define VERTICES_GET_Y(_vertices, _index) ((_vertices)[((_index) * 2) + 1])

/**
 * @brief Set the X coordinate in a vertices array.
 *
 * @param _vertices The vertices array.
 * @param _index The index of the point.
 * @param _x The new x coordinate.
 */
#define VERTICES_SET_X(_vertices, _index, _x) ((_vertices)[((_index) * 2) + 0] = (_x))

/**
 * @brief Set the Y coordinate in a vertices array.
 *
 * @param _vertices The vertices array.
 * @param _index The index of the point.
 * @param _y The new y coordinate.
 */
#define VERTICES_SET_Y(_vertices, _index, _y) ((_vertices)[((_index) * 2) + 1] = (_y))

/**
 * @brief Generate an array of vertices for a circle.
 *
 * @param vertices An array of x, y coordinate pairs to fill.
 * @param count The number of vertices to generate.
 * @param centerX The center x coordinate of the circle.
 * @param centerY The center y coordinate of the circle.
 * @param radius The radius of the circle.
 * @param start The start angle in radians.
 * @param end The end angle in radians.
 */
void vertices_circle(float* vertices, size_t count, float centerX, float centerY, float radius, float start, float end);

/**
 * @brief Rotate a polygon around a center point.
 *
 * @param vertices An array of x, y coordinate pairs.
 * @param count The number of vertices in the polygon.
 * @param angle The angle to rotate by, in radians.
 * @param centerX The center x coordinate to rotate around.
 * @param centerY The center y coordinate to rotate around.
 */
void vertices_rotate(float* vertices, size_t count, float angle, float centerX, float centerY);

/**
 * @brief Scale a polygon relative to a center point.
 *
 * @param vertices An array of x, y coordinate pairs.
 * @param count The number of vertices in the polygon.
 * @param scaleX The scale factor in the x direction.
 * @param scaleY The scale factor in the y direction.
 * @param centerX The center x coordinate to scale from.
 * @param centerY The center y coordinate to scale from.
 */
void vertices_scale(float* vertices, size_t count, float scaleX, float scaleY, float centerX, float centerY);

/**
 * @brief Translate a polygon by a given offset.
 *
 * @param vertices An array of x, y coordinate pairs.
 * @param count The number of vertices in the polygon.
 * @param offsetX The amount to translate in the x direction.
 * @param offsetY The amount to translate in the y direction.
 */
void vertices_translate(float* vertices, size_t count, float offsetX, float offsetY);

/**
 * @brief Check if a point is inside a polygon.
 *
 * @param vertices An array of x, y coordinate pairs.
 * @param count The number of vertices in the polygon.
 * @param x The x coordinate of the point.
 * @param y The y coordinate of the point.
 * @return `true` if the point is inside the polygon, `false` otherwise.
 */
bool vertices_contains(const float* vertices, size_t count, float x, float y);

/**
 * @brief Get the bounding box of a polygon.
 *
 * @param vertices An array of x, y coordinate pairs.
 * @param count The number of vertices in the polygon.
 * @param bounds A pointer to a rect_t to store the bounding box in.
 */
void vertices_bounds(const float* vertices, size_t count, rect_t* bounds);

/** @} */