#pragma once

#include <stdint.h>
#include <sys/fb.h>

#include "sysfs.h"

typedef struct fb fb_t;

typedef void* (*fb_mmap_t)(fb_t*, void*, uint64_t, prot_t);

typedef struct fb
{
    fb_info_t info;
    fb_mmap_t mmap;
} fb_t;

sysobj_t* fb_expose(fb_t* fb);
