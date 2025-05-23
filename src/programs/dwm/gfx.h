#pragma once

#include <libdwm/cmd.h>
#include <libdwm/pixel.h>
#include <libdwm/point.h>
#include <libdwm/rect.h>
#include <stdbool.h>
#include <stdint.h>

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

void gfx_transfer(gfx_t* dest, const gfx_t* src, const rect_t* destRect, const point_t* srcPoint);

void gfx_transfer_blend(gfx_t* dest, const gfx_t* src, const rect_t* destRect, const point_t* srcPoint);

void gfx_invalidate(gfx_t* gfx, const rect_t* rect);
