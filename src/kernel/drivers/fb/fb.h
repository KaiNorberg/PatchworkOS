#pragma once

#include <stdint.h>
#include <sys/fb.h>
#include <sys/proc.h>

#include "fs/sysfs.h"

typedef struct fb fb_t;

typedef void* (*fb_mmap_t)(fb_t*, void*, uint64_t, prot_t);

typedef struct fb
{
    fb_info_t info;
    fb_mmap_t mmap;
    sysfile_t sysfile;
} fb_t;

uint64_t fb_expose(fb_t* fb);
