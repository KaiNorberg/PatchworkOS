#ifndef _SYS_FB_H
#define _SYS_FB_H 1

#include <stdint.h>

typedef enum
{
    FB_ARGB32,
} fb_format_t;

typedef struct
{
    uint64_t width;
    uint64_t height;
    uint64_t stride;
    fb_format_t format;
} fb_info_t;

#define IOCTL_FB_INFO 0

#endif
