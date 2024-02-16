#pragma once

#include <stdint.h>

#define GOP_PUT(framebuffer, point, pixel) \
    *((Pixel*)((uint64_t)framebuffer->base + point.x * sizeof(Pixel) + point.y * framebuffer->pixelsPerScanline * 4)) = pixel

typedef struct
{
    uint32_t x;
    uint32_t y;
} Point;