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

#define PSF1_MAGIC 0x0436
#define PSF2_MAGIC 0x864AB572
#define PSF1_MODE_512 (1 << 0)

#define FBMP_MAGIC 0x706D6266

typedef enum
{
    GFX_GRADIENT_VERTICAL,
    GFX_GRADIENT_HORIZONTAL,
    GFX_GRADIENT_DIAGONAL
} gfx_gradient_type_t;

typedef enum gfx_align
{
    GFX_CENTER = 0,
    GFX_MAX = 1,
    GFX_MIN = 2,
} gfx_align_t;

typedef struct fbmp
{
    uint32_t magic;
    uint32_t width;
    uint32_t height;
    pixel_t data[];
} gfx_fbmp_t;

typedef struct gfx_psf
{
    uint32_t width;
    uint32_t height;
    uint32_t glyphSize;
    uint32_t glyphAmount;
    uint8_t glyphs[];
} gfx_psf_t;

typedef struct gfx
{
    pixel_t* buffer;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    rect_t invalidRect;
} gfx_t;

#define RECT_INIT_GFX(gfx) \
    (rect_t){ \
        0, \
        0, \
        (gfx)->width, \
        (gfx)->height, \
    };

gfx_fbmp_t* gfx_fbmp_new(const char* path);

gfx_psf_t* gfx_psf_new(const char* path);

void gfx_fbmp(gfx_t* gfx, const gfx_fbmp_t* fbmp, const point_t* point);

void gfx_fbmp_alpha(gfx_t* gfx, const gfx_fbmp_t* fbmp, const point_t* point);

void gfx_char(gfx_t* gfx, const gfx_psf_t* psf, const point_t* point, uint64_t height, char chr, pixel_t foreground,
    pixel_t background);

void gfx_text(gfx_t* gfx, const gfx_psf_t* psf, const rect_t* rect, gfx_align_t xAlign, gfx_align_t yAlign, uint64_t height,
    const char* str, pixel_t foreground, pixel_t background);

void gfx_text_multiline(gfx_t* gfx, const gfx_psf_t* psf, const rect_t* rect, gfx_align_t xAlign, gfx_align_t yAlign,
    uint64_t height, const char* str, pixel_t foreground, pixel_t background);

void gfx_rect(gfx_t* gfx, const rect_t* rect, pixel_t pixel);

void gfx_gradient(gfx_t* gfx, const rect_t* rect, pixel_t start, pixel_t end, gfx_gradient_type_t type, bool addNoise);

void gfx_edge(gfx_t* gfx, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void gfx_ridge(gfx_t* gfx, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

void gfx_rim(gfx_t* gfx, const rect_t* rect, uint64_t width, pixel_t pixel);

void gfx_scroll(gfx_t* gfx, const rect_t* rect, uint64_t offset, pixel_t background);

void gfx_transfer(gfx_t* dest, const gfx_t* src, const rect_t* destRect, const point_t* srcPoint);

void gfx_transfer_blend(gfx_t* dest, const gfx_t* src, const rect_t* destRect, const point_t* srcPoint);

void gfx_swap(gfx_t* dest, const gfx_t* src, const rect_t* rect);

void gfx_invalidate(gfx_t* gfx, const rect_t* rect);

#if defined(__cplusplus)
}
#endif

#endif
