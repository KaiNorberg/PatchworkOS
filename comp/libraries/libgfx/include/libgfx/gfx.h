#pragma once

#include <libgfx/pixel.h>
#include <libgfx/poly.h>
#include <libgfx/rect.h>
#include <libgfx/region.h>
#include <libgfx/verts.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * @brief Low-level 2D software graphics library
 * @defgroup comp_libgfx libgfx
 * @ingroup comp
 *
 * @{
 */

/**
 * @brief Blending modes for drawing.
 * @enum gfx_blend_t
 */
typedef enum
{
    GFX_BLEND_NONE,
    GFX_BLEND_ALPHA,
    GFX_BLEND_ADDITIVE,
    GFX_BLEND_MULTIPLY,
    GFX_BLEND_SET,
} gfx_blend_t;

/**
 * @brief Graphics context structure.
 * @struct gfx_t
 */
typedef struct gfx
{
    gfx_pixel_t* buffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
} gfx_t;

/**
 * @brief Create a drawing context.
 *
 * @param _buffer The pixel buffer.
 * @param _width The width of the buffer.
 * @param _height The height of the buffer.
 * @param _pitch The pitch (bytes per row) of the buffer.
 */
#define GFX(_buffer, _width, _height, _pitch) \
    (gfx_t) \
    { \
        .buffer = (_buffer), .width = (_width), .height = (_height), .pitch = (_pitch) \
    }

/**
 * @brief Get a pointer to a pixel in a drawing context.
 *
 * @param _draw The drawing context.
 * @param _x The x coordinate.
 * @param _y The y coordinate.
 */
#define GFX_GET_PIXEL(_draw, _x, _y) \
    ((gfx_pixel_t*)((uintptr_t)(_draw)->buffer + ((_y) * (_draw)->pitch) + ((_x) * sizeof(gfx_pixel_t))))

/**
 * @brief Draw a filled rectangle.
 *
 * @param draw The drawing context to draw on.
 * @param rect The rectangle to draw.
 * @param color The color to draw the rectangle with.
 */
void gfx_draw_rect(gfx_t* draw, gfx_rect_t rect, gfx_pixel_t color);

/**
 * @brief Draw a filled rounded rectangle.
 *
 * @param draw The drawing context to draw on.
 * @param rect The rectangle to draw.
 * @param radius The radius of the corners.
 * @param color The color to draw the rectangle with.
 * @param blend The blending mode to use.
 */
void gfx_draw_smooth_rect(gfx_t* draw, gfx_rect_t rect, int32_t radius, gfx_pixel_t color, gfx_blend_t blend);

/**
 * @brief Draw a filled rounded rectangle with a border.
 *
 * @param draw The drawing context to draw on.
 * @param rect The rectangle to draw.
 * @param radius The radius of the corners.
 * @param thickness The thickness of the border.
 * @param fillColor The color to draw the filled area with.
 * @param borderColor The color to draw the border with.
 * @param blend The blending mode to use.
 */
void gfx_draw_smooth_rect_border(gfx_t* draw, gfx_rect_t rect, int32_t radius, int32_t thickness, gfx_pixel_t fillColor,
    gfx_pixel_t borderColor, gfx_blend_t blend);

/**
 * @brief Fill the entire context with a color.
 *
 * @param draw The drawing context to fill.
 * @param color The color to fill the context with.
 */
static inline void gfx_draw_fill(gfx_t* draw, gfx_pixel_t color)
{
    gfx_rect_t rect = {0, 0, draw->width, draw->height};
    gfx_draw_rect(draw, rect, color);
}

/**
 * @brief Copy a region from one drawing context to another without alpha blending.
 *
 * @param dst The destination drawing context.
 * @param src The source drawing context.
 * @param dstRect The destination rectangle.
 * @param srcRect The source rectangle.
 */
void gfx_draw_copy(gfx_t* dst, gfx_t* src, gfx_rect_t dstRect, gfx_rect_t srcRect);

/**
 * @brief Transfer the entire source context to the destination context.
 *
 * @param dst The destination drawing context.
 * @param src The source drawing context.
 */
static inline void gfx_draw_transfer(gfx_t* dst, gfx_t* src)
{
    gfx_draw_copy(dst, src, GFX_RECT(0, 0, dst->width, dst->height), GFX_RECT(0, 0, src->width, src->height));
}

/**
 * @brief Copy a region from one drawing context to another with alpha blending.
 *
 * @param dst The destination drawing context.
 * @param src The source drawing context.
 * @param dstRect The destination rectangle.
 * @param srcRect The source rectangle.
 */
void gfx_draw_blit(gfx_t* dst, gfx_t* src, gfx_rect_t dstRect, gfx_rect_t srcRect);

/**
 * @brief Draw a polygon with anti-aliasing.
 *
 * @param draw The drawing context to draw on.
 * @param polygon The parsed polygon to draw.
 * @param color The color to draw the polygon with.
 * @param blend The blending mode to use.
 */
void gfx_draw_polygon(gfx_t* draw, gfx_poly_t* polygon, gfx_pixel_t color, gfx_blend_t blend);

/** @} */
