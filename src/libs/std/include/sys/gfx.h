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
    uint64_t width;
    uint64_t height;
    uint64_t stride;
} surface_t;

void gfx_psf_char(surface_t* surface, const psf_t* psf, const point_t* point, char chr);

void gfx_psf_string(surface_t* surface, const psf_t* psf, const point_t* point, const char* string);

void gfx_rect(surface_t* surface, const rect_t* rect, pixel_t pixel);

void gfx_edge(surface_t* surface, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

#if defined(__cplusplus)
}
#endif

#endif
