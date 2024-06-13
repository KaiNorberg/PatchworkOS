#ifndef _SYS_GFX_H
#define _SYS_GFX_H 1

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/pixel_t.h"
#include "_AUX/rect_t.h"

void gfx_rect(pixel_t* buffer, uint64_t width, const rect_t* rect, pixel_t pixel);

void gfx_edge(pixel_t* buffer, uint64_t width, const rect_t* rect, uint64_t edgeWidth, pixel_t foreground, pixel_t background);

#if defined(__cplusplus)
}
#endif

#endif
