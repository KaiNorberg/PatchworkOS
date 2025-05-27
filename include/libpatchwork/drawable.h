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

typedef struct drawable
{
    display_t* disp;
    uint32_t stride;
    pixel_t* buffer;
    rect_t contentRect;
    rect_t invalidRect;
} drawable_t;

typedef enum
{
    ALIGN_CENTER = 0,
    ALIGN_MAX = 1,
    ALIGN_MIN = 2,
} align_t;

typedef struct drawable drawable_t;
typedef struct image image_t;

void draw_content_rect(drawable_t* draw, rect_t* dest);

void draw_rect(drawable_t* draw, const rect_t* rect, pixel_t pixel);

void draw_outline(drawable_t* draw, const rect_t* rect, pixel_t pixel, uint32_t length, uint32_t width);

void draw_frame(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void draw_gradient(drawable_t* draw, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type,
    bool addNoise);

// The destRect is the rectangle that will be filled in the destination, the srcPoint is the starting point in the
// source to copy from.
void draw_transfer(drawable_t* dest, drawable_t* src, const rect_t* destRect, const point_t* srcPoint);

void draw_image(drawable_t* draw, image_t* image, const rect_t* destRect, const point_t* srcPoint);

void draw_transfer_blend(drawable_t* dest, drawable_t* src, const rect_t* destRect, const point_t* srcPoint);

void draw_image_blend(drawable_t* draw, image_t* image, const rect_t* destRect, const point_t* srcPoint);

void draw_bezel(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t pixel);

void draw_string(drawable_t* draw, const font_t* font, const point_t* point, pixel_t pixel, const char* string,
    uint64_t length);

void draw_text(drawable_t* draw, const rect_t* rect, const font_t* font, align_t xAlign, align_t yAlign, pixel_t pixel,
    const char* text);

void draw_text_multiline(drawable_t* draw, const rect_t* rect, const font_t* font, align_t xAlign, align_t yAlign,
    pixel_t pixel, const char* text);

void draw_ridge(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void draw_invalidate(drawable_t* draw, const rect_t* rect);

#if defined(__cplusplus)
}
#endif

#endif
