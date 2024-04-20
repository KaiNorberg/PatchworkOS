#pragma once

#include <common/boot_info/boot_info.h>

#include "sysfs/sysfs.h"

typedef struct
{
    Resource base;
    void* buffer;
    uint64_t size;
    uint64_t width;
    uint64_t height;
    uint64_t pixelsPerScanline;
    uint8_t bytesPerPixel;
    uint8_t blueOffset;
    uint8_t greenOffset;
    uint8_t redOffset;
    uint8_t alphaOffset;
} Framebuffer;

void renderer_init(GopBuffer* gopBuffer);