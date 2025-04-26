#pragma once

#include <stdint.h>
#include <sys/fb.h>

// TODO: This system is very basic and needs to be expanded.

typedef struct fb fb_t;

typedef uint32_t fb_pixel_t;

typedef uint64_t(*fb_flush_t)(fb_t*, const fb_pixel_t*, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

typedef struct fb
{
    uint64_t width;
    uint64_t height;
    fb_flush_t flush;
} fb_t;

uint64_t fb_expose(fb_t* fb);
