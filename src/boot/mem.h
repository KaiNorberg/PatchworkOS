#pragma once

#include <stdint.h>

#include <boot/boot_info.h>
#include <common/paging_types.h>

/**
 * @brief Basic memory managment.
 * @defgroup boot_mem Memory Management
 * @ingroup boot
 *
 * Handles the loading of the UEFI memory map and setting up a basic memory allocator for after we have exited boot
 * services but before we jump to the kernel as we cant use the normal memory allocator after exiting boot services.
 *
 * @{
 */

/**
 * @brief The minimum amount of pages that we will reserve for the basic allocator.
 */
#define MEM_BASIC_ALLOCATOR_MIN_PAGES 8192

/**
 * @brief The percentage of available memory that we will reserve for the basic allocator.
 *
 * This is rounded up to at least `MEM_BASIC_ALLOCATOR_MIN_PAGES`.
 */
#define MEM_BASIC_ALLOCATOR_RESERVE_PERCENTAGE 5

/**
 * @brief Initializes the basic memory allocator.
 *
 * @return On success, `EFI_SUCCESS`. On failure, an error code.
 */
EFI_STATUS mem_init(void);

/**
 * @brief Initialize and load the memory map provided by the UEFI firmware.
 *
 * @param map The boot memory map structure to initialize.
 * @return On success, `EFI_SUCCESS`. On failure, an error code.
 */
EFI_STATUS mem_map_init(boot_memory_map_t* map);

/**
 * @brief Deinitializes the memory map and frees any allocated resources.
 *
 * @param map The boot memory map structure to deinitialize.
 */
void mem_map_deinit(boot_memory_map_t* map);

/**
 * @brief Initializes a page table for use by the kernel.
 *
 * This function sets up a new page table and maps the memory regions specified in the boot memory map, the graphics
 * output protocol (GOP) framebuffer, and the kernel itself.
 *
 * It is intended to be used after exiting UEFI boot services and will fill the screen with a solid color and halt the
 * CPU if it encounters an error, as there is very little we can do in that situation.
 *
 * @param table The page table to initialize.
 * @param map The boot memory map containing memory descriptors to be mapped.
 * @param gop The graphics output protocol information for mapping the framebuffer.
 * @param kernel The kernel information for mapping the kernel binary.
 */
void mem_page_table_init(page_table_t* table, boot_memory_map_t* map, boot_gop_t* gop, boot_kernel_t* kernel);

/** @} */
