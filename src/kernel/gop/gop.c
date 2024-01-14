#include "gop.h"

void gop_put(Framebuffer* framebuffer, const Point point, const Pixel pixel)
{
    if (point.x > framebuffer->width || point.y > framebuffer->height)
    {
        return;
    }

    Pixel* pixelPtr = (Pixel*)((uint64_t)framebuffer->base + point.x * sizeof(Pixel) + point.y * framebuffer->pixelsPerScanline * 4);

    *pixelPtr = pixel;
}