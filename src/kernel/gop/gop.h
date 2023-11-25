#pragma once

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint8_t B;
    uint8_t G;
    uint8_t R;
    uint8_t A;
} Pixel;

typedef struct
{
    uint32_t X;
    uint32_t Y;
} Point;

typedef struct __attribute__((packed))
{
    Pixel* Base;
	uint64_t Size;
	uint32_t Width;
	uint32_t Height;
	uint32_t PixelsPerScanline;
} Framebuffer;

void gop_put(Framebuffer* framebuffer, const Point point, const Pixel pixel);