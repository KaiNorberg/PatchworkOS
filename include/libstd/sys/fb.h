#ifndef _SYS_FB_H
#define _SYS_FB_H 1

#include <stdint.h>
#include <sys/io.h>

/**
 * @brief Framebuffer device header.
 * @ingroup libstd
 * @defgroup libstd_sys_fb Framebuffer device
 *
 * The `sys/fb.h` header defines structs and constants used by framebuffer devices, for example `/dev/fb0`. The primary
 * way to use a framebuffer device is to first use `IOCTL_FB_INFO` to retrieve its width and height, then factoring in
 * its format to get the total size in bytes of the framebuffer and finally using `mmap` to map it to the currently
 * running processes address space.
 *
 * @{
 */

/**
 * @brief Framebuffer format enum.
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
 *
 * The `fb_info_t` struct stores information about a framebuffer and is retrieved by reading a `/dev/fb/[id]/info` file.
 *
 */
typedef struct
{
    uint64_t width;
    uint64_t height;
    uint64_t stride;
    fb_format_t format;
    char name[MAX_NAME];
} fb_info_t;

#endif

/** @} */
