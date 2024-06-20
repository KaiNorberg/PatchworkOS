#ifndef _SYS_GFX_H
#define _SYS_GFX_H 1

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/pixel_t.h"
#include "_AUX/point_t.h"
#include "_AUX/rect_t.h"

#define PSF_HEIGHT 16
#define PSF_WIDTH 8

typedef struct psf
{
    pixel_t foreground;
    pixel_t background;
    uint8_t scale;
    uint8_t* glyphs;
} psf_t;

typedef struct surface
{
    pixel_t* buffer;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    rect_t invalidArea;
} surface_t;

void gfx_invalidate(surface_t* surface, const rect_t* rect);

void gfx_psf_char(surface_t* surface, const psf_t* psf, const point_t* point, char chr);

void gfx_psf_string(surface_t* surface, const psf_t* psf, const point_t* point, const char* string);

void gfx_rect(surface_t* surface, const rect_t* rect, pixel_t pixel);

void gfx_edge(surface_t* surface, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void gfx_transfer(surface_t* dest, const surface_t* src, const rect_t* destRect, const point_t* srcPoint);

#if defined(__cplusplus)
}
#endif

#endif
