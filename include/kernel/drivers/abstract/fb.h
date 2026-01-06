#pragma once

#include <_internal/MAX_PATH.h>
#include <kernel/fs/devfs.h>
#include <kernel/mem/vmm.h>

#include <stdint.h>
#include <sys/proc.h>

typedef struct fb fb_t;

/**
 * @brief Framebuffer abstraction.
 * @defgroup kernel_drivers_abstract_fb Framebuffer Abstraction
 * @ingroup kernel_drivers_abstract
 *
 * Framebuffer devices are exposed as a `/dev/fb/[id]/` directory, containing the below files.
 *
 * ## name
 *
 * A read-only file that contains the driver defined name of the framebuffer device.
 *
 * ## info
 *
 * A read-only file that contains information about the framebuffer in the format
 *
 * ```
 * [width] [height] [pitch] [format]
 * ```
 *
 * where `width` and `height` are the integer dimensions of the framebuffer in pixels, `pitch` is the integer number of
 * bytes per row, and `format` is the pixel format presented as a series of letter number pairs in little-endian order
 * (starting from the lowest memory address).
 *
 * For example, `1920 1080 7680 B8G8R8A8` represents a 1920x1080 framebuffer with a pitch of 7680 bytes in 32-bit ARGB
 * format.
 *
 * ## data
 *
 * A readable, writable and mappable file that represents the actual framebuffer memory. Writing to this file updates
 * the pixels on the screen and reading from it retrieves the current pixel data.
 *
 * @{
 */

/**
 * @brief Framebuffer information.
 * @struct fb_info_t
 */
typedef struct fb_info
{
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    char format[MAX_PATH];
} fb_info_t;

/**
 * @brief Framebuffer operations.
 * @struct fb_ops_t
 */
typedef struct fb_ops
{
    uint64_t (*info)(fb_t* fb, fb_info_t* info);
    uint64_t (*read)(fb_t* fb, void* buffer, uint64_t count, uint64_t* offset);
    uint64_t (*write)(fb_t* fb, const void* buffer, uint64_t count, uint64_t* offset);
    void* (*mmap)(fb_t* fb, void* address, uint64_t length, uint64_t* offset, pml_flags_t flags);
    void (*cleanup)(fb_t* fb);
} fb_ops_t;

/**
 * @brief Framebuffer structure.
 * @struct fb_t
 */
typedef struct fb
{
    char name[MAX_PATH];
    const fb_ops_t* ops;
    void* private;
    dentry_t* dir;
    list_t files;
} fb_t;

/**
 * @brief Allocate and initialize a new framebuffer.
 *
 * @param name The driver specified name of the framebuffer.
 * @param ops The operations for the framebuffer.
 * @param private Private data for the framebuffer.
 * @return On success, the new framebuffer. On failure, `NULL` and `errno` is set.
 */
fb_t* fb_new(const char* name, const fb_ops_t* ops, void* private);

/**
 * @brief Frees a framebuffer.
 *
 * @param fb The framebuffer to free.
 */
void fb_free(fb_t* fb);

/** @} */
