#pragma once

#include <libdraw/pixel.h>
#include <libdraw/polygon.h>
#include <libdraw/rect.h>
#include <libdraw/vertices.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @brief Low-level drawing library
 * @defgroup comp_libdraw libdraw
 * @ingroup comp
 *
 * @{
 */

/**
 * @brief Drawing context structure.
 * @struct drawable_t
 */
typedef struct drawable
{
    pixel_t* buffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} drawable_t;

/**
 * @brief Create a rectangle from a drawing context.
 *
 * @param _draw The drawing context.
 */
#define RECT_FROM_DRAWABLE(_draw) \
    (rect_t) \
    { \
        0, 0, (int32_t)(_draw)->width, (int32_t)(_draw)->height \
    }

/**
 * @brief Get a pointer to a pixel in a drawing context.
 *
 * @param _draw The drawing context.
 * @param _x The x coordinate.
 * @param _y The y coordinate.
 */
#define DRAW_GET_PIXEL(_draw, _x, _y) \
    ((pixel_t*)((uintptr_t)(_draw)->buffer + ((_y) * (_draw)->pitch) + ((_x) * sizeof(pixel_t))))

/**
 * @brief Initialize a drawing context.
 *
 * @param draw The drawing context to initialize.
 * @param buffer The pixel buffer.
 * @param width The width of the buffer in pixels.
 * @param height The height of the buffer in pixels.
 * @param pitch The amount of bytes to advance to the next row.
 */
static inline void draw_init(drawable_t* draw, pixel_t* buffer, size_t width, size_t height, size_t pitch)
{
    draw->buffer = buffer;
    draw->width = width;
    draw->height = height;
    draw->pitch = pitch;
}

/**
 * @brief Draw a filled rectangle.
 *
 * @param draw The drawing context to draw on.
 * @param rect The rectangle to draw.
 * @param color The color to draw the rectangle with.
 */
void draw_rect(drawable_t* draw, rect_t rect, pixel_t color);

/**
 * @brief Fill the entire context with a color.
 *
 * @param draw The drawing context to fill.
 * @param color The color to fill the context with.
 */
static inline void draw_fill(drawable_t* draw, pixel_t color)
{
    rect_t rect = {0, 0, draw->width, draw->height};
    draw_rect(draw, rect, color);
}

/**
 * @brief Copy a region from one drawing context to another without alpha blending.
 *
 * @param dst The destination drawing context.
 * @param src The source drawing context.
 * @param dstRect The destination rectangle.
 * @param srcRect The source rectangle.
 */
void draw_copy(drawable_t* dst, drawable_t* src, rect_t dstRect, rect_t srcRect);

/**
 * @brief Transfer the entire source context to the destination context.
 *
 * @param dst The destination drawing context.
 * @param src The source drawing context.
 */
static inline void draw_transfer(drawable_t* dst, drawable_t* src)
{
    draw_copy(dst, src, RECT_FROM_DRAWABLE(dst), RECT_FROM_DRAWABLE(src));
}

/**
 * @brief Copy a region from one drawing context to another with alpha blending.
 *
 * @param dst The destination drawing context.
 * @param src The source drawing context.
 * @param dstRect The destination rectangle.
 * @param srcRect The source rectangle.
 */
void draw_blit(drawable_t* dst, drawable_t* src, rect_t dstRect, rect_t srcRect);

/**
 * @brief Blending modes for polygon drawing.
 * @enum draw_blend_t
 */
typedef enum
{
    DRAW_BLEND_NONE,
    DRAW_BLEND_ALPHA,
    DRAW_BLEND_ADDITIVE,
    DRAW_BLEND_MULTIPLY,
    DRAW_BLEND_SET,
} draw_blend_t;

/**
 * @brief Draw a polygon with anti-aliasing.
 *
 * @param draw The drawing context to draw on.
 * @param polygon The parsed polygon to draw.
 * @param color The color to draw the polygon with.
 * @param blend The blending mode to use.
 */
void draw_polygon(drawable_t* draw, polygon_t* polygon, pixel_t color, draw_blend_t blend);

/** @} */
