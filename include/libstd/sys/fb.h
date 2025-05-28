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

/**
 * @brief Framebuffer device info ioctl.
 *
 * The ioctl request id for retrieving information about a framebuffer device, for example `sys:/fb0`, should be used
 * like `ioctl(fb, IOCTL_FB_INFO, &info, sizeof(fb_info_t))`.
 *
 * @return On success, returns 0, on failure, returns `ERR`.
 */
#define IOCTL_FB_INFO 0

#endif
