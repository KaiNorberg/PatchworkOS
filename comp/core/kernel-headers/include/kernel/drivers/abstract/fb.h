#pragma once

#include <_libc/MAX_PATH.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/mem/vmm.h>

#include <libc/proc.h>
#include <stdint.h>

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
 * @brief Framebuffer structure.
 * @struct fb_t
 */
typedef struct fb
{
    char* name;                ///< The name of the framebuffer.
    size_t width;              ///< The width in pixels.
    size_t height;             ///< The height in pixels.
    size_t pitch;              ///< The number of bytes per line.
    char* format;              ///< Specifies the format of the framebuffer.
    const vnode_class_t* data; ///< The class to use for the framebuffers data file.
    struct
    {
        dentry_t* dir;
        dentry_t* name;
        dentry_t* info;
        dentry_t* data;
    } internal;
} fb_t;

/**
 * @brief Initialize the framebuffer abstraction.
 */
void fb_init(void);

/**
 * @brief Register a new framebuffer.
 *
 * @param fb Pointer to the framebuffer to register.
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
