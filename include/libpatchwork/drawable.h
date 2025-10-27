#ifndef PATCHWORK_DRAW_H
#define PATCHWORK_DRAW_H 1

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
 * @brief Drawable.
 * @defgroup libpatchwork_draw Drawable
 * @ingroup libpatchwork
 *
 * A drawable implements a generic system for drawing operations to a pixel buffer.
 *
 * @{
 */

/**
 * @brief Drawable structure.
 * @struct drawable_t
 */
typedef struct drawable
{
    display_t* disp;
    uint32_t stride;
    pixel_t* buffer;
    rect_t contentRect;
    rect_t invalidRect;
} drawable_t;

/**
 * @brief Alignment type.
 * @typedef align_t
 */
typedef enum
{
    ALIGN_CENTER = 0,
    ALIGN_MAX = 1,
    ALIGN_MIN = 2,
} align_t;

/**
 * @brief Direction type.
 * @typedef direction_t
 */
typedef enum
{
    DIRECTION_VERTICAL,
    DIRECTION_HORIZONTAL,
    DIRECTION_DIAGONAL
} direction_t;

/**
 * @brief Draw a filled rectangle.
 *
 * Will fit the rectangle to the drawable's content rectangle.
 *
 * @param draw The drawable to draw to.
 * @param rect The rectangle to draw.
 * @param pixel The pixel color to fill with.
 */
void draw_rect(drawable_t* draw, const rect_t* rect, pixel_t pixel);

/**
 * @brief Draw a filled polygon.
 *
 * Will clip the polygon to fit within the drawable's content rectangle.
 *
 * @param draw The drawable to draw to.
 * @param points The points of the polygon.
 * @param pointCount The number of points in the polygon, must be at least 3.
 * @param pixel The pixel color to fill with.
 */
void draw_polygon(drawable_t* draw, const point_t* points, uint64_t pointCount, pixel_t pixel);

/**
 * @brief Draw a line between two points.
 *
 * Will clip the line to fit within the drawable's content rectangle.
 *
 * @param draw The drawable to draw to.
 * @param start The starting point of the line, inclusive.
 * @param end The ending point of the line, inclusive.
 * @param pixel The pixel color to draw with.
 * @param thickness The thickness of the line, must be at least 1.
 */
void draw_line(drawable_t* draw, const point_t* start, const point_t* end, pixel_t pixel, uint32_t thickness);

/**
 * @brief Draw a skeuomorphic frame.
 *
 * Will draw a frame with the given width, using the foreground color for the top and left sides, and the background
 * color for the bottom and right sides.
 *
 * Will fit the rectangle to the drawable's content rectangle.
 *
 * @param draw The drawable to draw to.
 * @param rect The rectangle to draw the frame around.
 * @param width The width of the frame.
 * @param foreground The pixel color for the top and left sides.
 * @param background The pixel color for the bottom and right sides.
 */
void draw_frame(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

/**
 * @brief Draw a dashed outline just inside the given rectangle.
 *
 * Will fit the rectangle to the drawable's content rectangle.
 *
 * @param draw The drawable to draw to.
 * @param rect The rectangle to draw the outline inside.
 * @param pixel The pixel color to draw with.
 * @param length The length of the dashes.
 * @param width The width of the outline.
 */
void draw_dashed_outline(drawable_t* draw, const rect_t* rect, pixel_t pixel, uint32_t length, int32_t width);

/**
 * @brief Draw a filled border bezel just inside the given rectangle.
 *
 * Will fit the rectangle to the drawable's content rectangle.
 *
 * @param draw The drawable to draw to.
 * @param rect The rectangle to draw the bezel around.
 * @param width The width of the bezel.
 * @param pixel The pixel color to draw with.
 */
void draw_bezel(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t pixel);

/**
 * @brief Draw a gradient filled rectangle.
 *
 * Will fit the rectangle to the drawable's content rectangle.
 *
 * @param draw The drawable to draw to.
 * @param rect The rectangle to draw.
 * @param start The starting pixel color.
 * @param end The ending pixel color.
 * @param direction The direction of the gradient.
 * @param shouldAddNoise Whether to add noise to the gradient to reduce banding.
 */
void draw_gradient(drawable_t* draw, const rect_t* rect, pixel_t start, pixel_t end, direction_t direction,
    bool shouldAddNoise);

/**
 * @brief Transfer pixels from one drawable to another.
 *
 * @param dest The destination drawable.
 * @param src The source drawable.
 * @param destRect The rectangle that will be filled in the destination.
 * @param srcPoint The top-left point in the source drawable to copy from.
 */
void draw_transfer(drawable_t* dest, drawable_t* src, const rect_t* destRect, const point_t* srcPoint);

/**
 * @brief Transfer pixels from one drawable to another with alpha blending.
 *
 * @param dest The destination drawable.
 * @param src The source drawable.
 * @param destRect The rectangle that will be filled in the destination.
 * @param srcPoint The top-left point in the source drawable to copy from.
 */
void draw_transfer_blend(drawable_t* dest, drawable_t* src, const rect_t* destRect, const point_t* srcPoint);

/**
 * @brief Draw an image,
 *
 * @param draw The drawable to draw to.
 * @param image The image to draw.
 * @param destRect The rectangle that will be filled in the drawable.
 * @param srcPoint The top-left point in the image to copy from.
 */
void draw_image(drawable_t* draw, image_t* image, const rect_t* destRect, const point_t* srcPoint);

/**
 * @brief Draw an image with alpha blending.
 *
 * @param draw The drawable to draw to.
 * @param image The image to draw.
 * @param destRect The rectangle that will be filled in the drawable.
 * @param srcPoint The top-left point in the image to copy from.
 */
void draw_image_blend(drawable_t* draw, image_t* image, const rect_t* destRect, const point_t* srcPoint);

/**
 * @brief Draw a string.
 *
 * Will not draw a background, only the glyphs of the string.
 *
 * @param draw The drawable to draw to.
 * @param font The font to use. If `NULL`, the default font for the display will be used.
 * @param point The top-left point to start drawing the string at.
 * @param pixel The pixel color to draw with.
 * @param string The string to draw, null-termination is ignored.
 * @param length The length of the string to draw.
 */
void draw_string(drawable_t* draw, const font_t* font, const point_t* point, pixel_t pixel, const char* string,
    uint64_t length);

/**
 * @brief Draw text to a drawable.
 *
 * Will clip the text to fit within the rectangle, adding an ellipsis (`...`) if the text is too long.
 *
 * @param draw The drawable to draw to.
 * @param rect The rectangle to draw the text within.
 * @param font The font to use. If `NULL`, the default font for the display will be used.
 * @param xAlign The horizontal alignment of the text within the rectangle.
 * @param yAlign The vertical alignment of the text within the rectangle.
 * @param pixel The pixel color to draw with.
 * @param text The text to draw, null-terminated.
 */
void draw_text(drawable_t* draw, const rect_t* rect, const font_t* font, align_t xAlign, align_t yAlign, pixel_t pixel,
    const char* text);

/**
 * @brief Draw multiline text to a drawable.
 *
 * Will wrap lines to fit within the rectangle. Newlines (`\n`) are also supported.
 *
 * @param draw The drawable to draw to.
 * @param rect The rectangle to draw the text within.
 * @param font The font to use. If `NULL`, the default font for the display will be used.
 * @param xAlign The horizontal alignment of the text within the rectangle.
 * @param yAlign The vertical alignment of the text within the rectangle.
 * @param pixel The pixel color to draw with.
 * @param text The text to draw, null-terminated.
 */
void draw_text_multiline(drawable_t* draw, const rect_t* rect, const font_t* font, align_t xAlign, align_t yAlign,
    pixel_t pixel, const char* text);

/**
 * @brief Draw a ridge effect.
 *
 * Will draw a inverted frame inside another frame inside the given rectangle, creating a ridge effect.
 *
 * @param draw The drawable to draw to.
 * @param rect The rectangle to draw the ridge effect within.
 * @param width The total width of the ridge effect.
 * @param foreground The pixel color for the top and left sides of the outer frame and the bottom and right sides of the
 * inner frame.
 * @param background The pixel color for the bottom and right sides of the outer frame and the top and left sides of the
 * inner frame.
 */
void draw_ridge(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

/**
 * @brief Draw a separator line.
 *
 * Will draw a separator line within the given rectangle, either horizontally or vertically.
 *
 * @param draw The drawable to draw to.
 * @param rect The rectangle to draw the separator within.
 * @param highlight The pixel color for the highlight side of the separator (top or left).
 * @param shadow The pixel color for the shadow side of the separator (bottom or right).
 * @param direction The direction of the separator line.
 */
void draw_separator(drawable_t* draw, const rect_t* rect, pixel_t highlight, pixel_t shadow, direction_t direction);

/**
 * @brief Invalidate a rectangle in the drawable.
 *
 * Marks the given rectangle as invalid, so that it will be updated on the next flush.
 *
 * Flushing is handled by each element or other system using the drawable, not the drawable itself.
 *
 * @param draw The drawable.
 * @param rect The rectangle to invalidate, or `NULL` to invalidate the entire content rectangle.
 */
void draw_invalidate(drawable_t* draw, const rect_t* rect);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
