#include "gop.h"

void gop_put(Framebuffer* framebuffer, const Point point, const Pixel pixel)
{
    if (point.X > (int32_t)framebuffer->Width || point.X < 0 || point.Y > (int32_t)framebuffer->Height || point.Y < 0)
    {
        return;
    }

    Pixel* pixelPtr = (Pixel*)((uint64_t)framebuffer->Base + point.X * sizeof(Pixel) + point.Y * framebuffer->PixelsPerScanline * 4);

    *pixelPtr = pixel;
}