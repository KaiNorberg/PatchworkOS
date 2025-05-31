#ifndef _SYS_FB_H
#define _SYS_FB_H 1

#include <stdint.h>

/**
 * @brief Framebuffer device header.
 * @ingroup libstd
 * @defgroup libstd_sys_fb Framebuffer device
 *
 * The `sys/fb.h` header defines structs and constants used by framebuffer devices, for example `sys:/fb0`. The primary
 * way to use a framebuffer device is to first use `IOCTL_FB_INFO` to retrieve its width and height, then factoring in
 * its format to get the total size in bytes of the framebuffer and finally using `mmap` to map it to the currently
 * running processes address space.
 *
 */

/**
 * @brief Framebuffer format enum.
 * @ingroup libstd_sys_fb
 *
 * The `fb_format_t` enum specified the format of the pixel data in a framebuffer. All byte orders are specified in
 * little-endian.
 *
 */
typedef enum
{
    FB_ARGB32,
} fb_format_t;

/**
 * @brief Framebuffer info struct.
 * @ingroup libstd_sys_fb
 *
 * The `fb_info_t` struct stores information about a framebuffer and is retrieved using the `IOCTL_FB_INFO` ioctl.
 *
 */
typedef struct
{
    uint64_t width;
    uint64_t height;
    uint64_t stride;
    fb_format_t format;
} fb_info_t;

/**
 * @brief Framebuffer device info ioctl.
 * @ingroup libstd_sys_fb
 *
 * The `IOCTL_FB_INFO` macro defines the ioctl request id for retrieving information about a framebuffer device, for
 * example `sys:/fb0`, should be used like `ioctl(fb, IOCTL_FB_INFO, &info, sizeof(fb_info_t))`.
 *
 * @return On success, returns 0, on failure, returns `ERR`.
 */
#define IOCTL_FB_INFO 0

#endif
