#pragma once

#include <common/boot_info.h>

#include <sys/ioctl.h>

#include "sysfs.h"

typedef struct
{
    Resource base;
    void* buffer;
    ioctl_fb_info_t info;
} Framebuffer;

void renderer_init(GopBuffer* gopBuffer);