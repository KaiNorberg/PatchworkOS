#pragma once

#include <stdint.h>
#include <sys/fb.h>
#include <sys/proc.h>

#include "fs/sysfs.h"
#include "mem/vmm.h"

typedef struct fb fb_t;

/**
 * @brief Framebuffer driver abstraction.
 * @defgroup kernel_drivers_abstractions_fb Framebuffer Abstraction
 * @ingroup kernel_drivers_abstractions
 *
 * Framebuffer devices are exposed as a `/dev/fb/[id]` directory, containing the following files:
 * - `buffer`: A file that can be `mmap`ed to access the framebuffer memory.
 * - `info`: A read-only file that contains the `fb_info_t` struct for the framebuffer.
 * - `name`: A read-only file that contains the framebuffer driver specified name (e.g. "GOP")
 *
 * @{
 */

/**
 * @brief Framebuffer mmap callback type.
 */
typedef void* (*fb_mmap_t)(fb_t*, void*, uint64_t, pml_flags_t);

/**
 * @brief Framebuffer structure.
 * @struct fb_t
 */
typedef struct fb
{
    char id[MAX_NAME];
    char name[MAX_NAME];
    fb_info_t info;
    fb_mmap_t mmap;
    dentry_t* dir;
    dentry_t* bufferFile;
    dentry_t* infoFile;
    dentry_t* nameFile;
} fb_t;

/**
 * @brief Allocate and initialize a framebuffer structure.
 *
 * Will make the framebuffer available under `/dev/fb/[id]`.
 *
 * @param info Pointer to the framebuffer information.
 * @param mmap Function that user space will invoke to mmap the framebuffer.
 * @param name Name of the framebuffer device.
 * @return On success, the new framebuffer structure. On failure, `NULL` and `errno` is set.
 */
fb_t* fb_new(const fb_info_t* info, fb_mmap_t mmap, const char* name);

/**
 * @brief Free and deinitialize a framebuffer structure.
 *
 * Removes the framebuffer from `/dev/fb/[id]`.
 *
 * @param fb Pointer to the framebuffer structure to free.
 */
void fb_free(fb_t* fb);

/** @} */
