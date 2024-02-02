#pragma once

#include <stdint.h>

#define GOP_PUT(framebuffer, point, pixel) \
    *((Pixel*)((uint64_t)framebuffer->base + point.x * sizeof(Pixel) + point.y * framebuffer->pixelsPerScanline * 4)) = pixel

typedef struct __attribute__((packed))
{
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
} Pixel;

typedef struct
{
    uint32_t x;
    uint32_t y;
} Point;

typedef struct __attribute__((packed))
{
    Pixel* base;
	uint64_t size;
	uint32_t width;
	uint32_t height;
	uint32_t pixelsPerScanline;
} Framebuffer;