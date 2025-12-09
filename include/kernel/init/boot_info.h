#pragma once

#include <kernel/defs.h>
#include <boot/boot_info.h>

/**
 * @brief Kernel-side boot information handling.
 * @defgroup kernel_init_boot_info Boot Information
 * @ingroup kernel_init
 *
 * @{
 */

/**
 * @brief Gets the boot info structure.
 * 
 * @return The boot info structure.
 */
boot_info_t* boot_info_get(void);

/**
 * @brief Offset all pointers in the boot info structure to the higher half.
 */
void boot_info_to_higher_half(void);

/**
 * @brief Frees the boot info structure and all its associated data.
 */
void boot_info_free(void);

/** @} */