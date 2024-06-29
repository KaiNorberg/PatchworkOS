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

#define FBMP_MAGIC 0x706D6266

typedef struct fbmp
{
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    pixel_t data[];
} fbmp_t;

typedef struct __attribute__((packed))
{
    uint16_t magic;
    uint8_t mode;
    uint8_t charSize;
} psf_header_t;

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

#define RECT_INIT_SURFACE(surface) \
    (rect_t){ \
        0, \
        0, \
        (surface)->width, \
        (surface)->height, \
    };

fbmp_t* gfx_load_fbmp(const char* path);

void gfx_fbmp(surface_t* surface, const fbmp_t* fbmp, const point_t* point);

void gfx_psf_char(surface_t* surface, const psf_t* psf, const point_t* point, char chr);

void gfx_psf_string(surface_t* surface, const psf_t* psf, const point_t* point, const char* string);

void gfx_rect(surface_t* surface, const rect_t* rect, pixel_t pixel);

void gfx_edge(surface_t* surface, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void gfx_ridge(surface_t* surface, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void gfx_transfer(surface_t* dest, const surface_t* src, const rect_t* destRect, const point_t* srcPoint);

void gfx_transfer_blend(surface_t* dest, const surface_t* src, const rect_t* destRect, const point_t* srcPoint);

void gfx_swap(surface_t* dest, const surface_t* src, const rect_t* rect);

void gfx_invalidate(surface_t* surface, const rect_t* rect);

#if defined(__cplusplus)
}
#endif

#endif
