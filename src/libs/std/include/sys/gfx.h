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

typedef struct surface
{
    pixel_t* buffer;
    uint64_t width;
    uint64_t height;
    uint64_t stride;
} surface_t;

void gfx_rect(surface_t* surface, const rect_t* rect, pixel_t pixel);

void gfx_edge(surface_t* surface, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background);

#if defined(__cplusplus)
}
#endif

#endif
