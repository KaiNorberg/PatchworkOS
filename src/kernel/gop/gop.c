#include "gop.h"

void gop_put(Framebuffer* framebuffer, const Point point, const Pixel pixel)
{
    if (point.x > (int32_t)framebuffer->width || point.x < 0 || point.y > (int32_t)framebuffer->height || point.y < 0)
    {
        return;
    }

    Pixel* pixelPtr = (Pixel*)((uint64_t)framebuffer->base + point.x * sizeof(Pixel) + point.y * framebuffer->pixelsPerScanline * 4);

    *pixelPtr = pixel;
}