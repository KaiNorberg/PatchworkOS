#pragma once

#include <_libstd/MAX_PATH.h>
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
    size_t width;
    size_t height;
    size_t pitch;
    char format[MAX_PATH];
} fb_info_t;

/**
 * @brief Framebuffer structure.
 * @struct fb_t
 */
typedef struct fb
{
    char* name;
    status_t (*info)(fb_t* fb, fb_info_t* info);
    status_t (*mmap)(fb_t* fb, void** address, size_t length, size_t* offset, pml_flags_t flags);
    status_t (*read)(fb_t* fb, void* buffer, size_t count, size_t* offset, size_t* bytesRead);
    status_t (*write)(fb_t* fb, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten);
    void (*cleanup)(fb_t* fb);
    void* data;
    dentry_t* dir;
    list_t files;
} fb_t;

/**
 * @brief Register a new framebuffer.
 *
 * @param fb Pointer to the framebuffer structure to initialize.
 * @return An appropriate status value.
 */
status_t fb_register(fb_t* fb);

/**
 * @brief Unregister a framebuffer.
 *
 * @param fb The framebuffer to unregister.
 */
void fb_unregister(fb_t* fb);

/** @} */
