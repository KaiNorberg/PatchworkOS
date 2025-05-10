#ifndef DWM_DRAW_H
#define DWM_DRAW_H 1

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

typedef enum
{
    ALIGN_CENTER = 0,
    ALIGN_MAX = 1,
    ALIGN_MIN = 2,
} align_t;

typedef struct drawable drawable_t;
typedef struct image image_t;

void draw_rect(drawable_t* draw, const rect_t* rect, pixel_t pixel);

void draw_rect(drawable_t* draw, const rect_t* rect, pixel_t pixel);

void draw_edge(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void draw_gradient(drawable_t* draw, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type,
    bool addNoise);

// The destRect is the rectangle that will be filled in the destination, the srcPoint is the starting point in the
// source to copy from.
void draw_transfer(drawable_t* dest, drawable_t* src, const rect_t* destRect, const point_t* srcPoint);

void draw_buffer(drawable_t* draw, pixel_t* buffer, uint64_t index, uint64_t length);

void draw_image(drawable_t* draw, image_t* image, const rect_t* destRect, const point_t* srcPoint);

void draw_rim(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t pixel);

void draw_string(drawable_t* draw, font_t* font, const point_t* point, pixel_t foreground, pixel_t background,
    const char* string, uint64_t length);

void draw_text(drawable_t* draw, const rect_t* rect, font_t* font, align_t xAlign, align_t yAlign, pixel_t foreground,
    pixel_t background, const char* text);

void draw_text_multiline(drawable_t* draw, const rect_t* rect, font_t* font, align_t xAlign, align_t yAlign,
    pixel_t foreground, pixel_t background, const char* text);

void draw_ridge(drawable_t* draw, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void draw_invalidate(drawable_t* draw, const rect_t* rect);

#if defined(__cplusplus)
}
#endif

#endif
