#pragma once

#include "sync/rwmutex.h"
#include "utils/bitmap.h"

#include <boot/boot_info.h>
#include <common/paging_types.h>

#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Address Space handling.
 * @defgroup kernel_mem_space Space
 * @ingroup kernel_mem
 *
 * @{
 */

/**
 * @brief Flags for space initialization.
 * @enum space_flags_t
 */
typedef enum
{
    SPACE_NONE = 0,
    /**
     * Use the PMM bitmap to allocate the page table, this is really only for the kernel page table as it
     * must be within a 32 bit boundary because the smp trampoline loads it as a dword.
     */
    SPACE_USE_PMM_BITMAP = 1 << 0,
    SPACE_MAP_KERNEL_BINARY = 1 << 1, ///< Map the kernel binary into the address space.
    SPACE_MAP_KERNEL_HEAP = 1 << 2,   ///< Map the kernel heap into the address space.
    SPACE_MAP_IDENTITY = 1 << 3,      ///< Map the identity mapped physical memory into the address space.
} space_flags_t;

/**
 * @brief Space callback function.
 */
typedef void (*space_callback_func_t)(void* private);

/**
 * @brief Space callback structure.
 * @struct vmm_callback_t
 */
typedef struct
{
    space_callback_func_t func;
    void* private;
    uint64_t pageAmount;
} space_callback_t;

/**
 * @brief Virtual address space structure.
 * @struct space_t
 *
 * The `space_t` structure represents a virtual address space.
 */
typedef struct space
{
    page_table_t pageTable; ///< The page table associated with the address space.
    uintptr_t endAddress;   ///< The end address for allocations in this address space.
    uintptr_t freeAddress;  ///< The next available free virtual address in this address space.
    space_flags_t flags;    ///< Flags for the address space.
    /**
     * Array of callbacks for this address space, indexed by the callback ID.
     */
    space_callback_t callbacks[PML_MAX_CALLBACK];
    bitmap_t callbackBitmap; ///< Bitmap to track available callback IDs.
    /**
     * Buffer for the callback bitmap, see `bitmap_t` for more info.
     */
    uint64_t bitmapBuffer[BITMAP_BITS_TO_QWORDS(PML_MAX_CALLBACK)];
    /**
     * Mutex to protect this structure and its mappings.
     *
     * Should be acquired in system calls that take in pointers to user space, to prevent TOCTOU attacks.
     */
    rwmutex_t mutex;
} space_t;

/**
 * @brief Initializes a virtual address space.
 *
 * @param space The address space to initialize.
 * @param startAddress The starting address for allocations in this address space.
 * @param endAddress The ending address for allocations in this address space.
 * @param flags Flags to control the initialization behavior.
 * @return On success, returns 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t space_init(space_t* space, uintptr_t startAddress, uintptr_t endAddress, space_flags_t flags);

/**
 * @brief Deinitializes a virtual address space.

 * @param space The address space to deinitialize.
 */
void space_deinit(space_t* space);

/**
 * @brief Loads a virtual address space.
 *
 * @param space The address space to load.
 */
void space_load(space_t* space);

/**
 * @brief Helper structure for managing address space mappings.
 * @struct space_mapping_t
 */
typedef struct
{
    void* virtAddr;
    void* physAddr;
    uint64_t pageAmount;
    pml_flags_t flags;
} space_mapping_t;

/**
 * @brief Prepare for changes to the address space mappings.
 *
 * Will return with the spaces mutex acquired for writing, which must be released by calling
 * `space_mapping_end()`.
 *
 * If `flags & PML_USER` then the addresses must be in the user space range.
 *
 * @param space The target address space.
 * @param mapping Will be filled with parsed information about the mapping.
 * @param virtAddr The desired virtual address. If `NULL`, the kernel chooses an available address.
 * @param physAddr The physical address to map from. Can be `NULL`.
 * @param length The length of the virtual memory region to allocate, in bytes.
 * @param flags The page table flags for the mapping.
 * @return On success, returns 0. On failure, returns `ERR`.
 */
uint64_t space_mapping_start(space_t* space, space_mapping_t* mapping, void* virtAddr, void* physAddr, uint64_t length,
    pml_flags_t flags);

/**
 * @brief Allocate a callback.
 *
 * When `pageAmount` number of pages with this callback ID are unmapped or the address space is freed,
 * the callback function will be called with the provided private data.
 *
 * @param space The target address space.
 * @param pageAmount The number of pages the callback is responsible for.
 * @param func The callback function.
 * @param private Private data to pass to the callback function.
 * @return On success, returns the callback ID. On failure, returns `PML_MAX_CALLBACK`.
 */
pml_callback_id_t space_alloc_callback(space_t* space, uint64_t pageAmount, space_callback_func_t func, void* private);

/**
 * @brief Free a callback.
 *
 * Allows the callback ID to be reused. The callback function will not be called.
 *
 * @param space The target address space.
 * @param callbackId The callback ID to free.
 */
void space_free_callback(space_t* space, pml_callback_id_t callbackId);

/**
 * @brief Performs cleanup after changes to the address space mappings.
 *
 * @param space The target address space.
 * @param mapping The parsed information about the mapping.
 * @param err The error code, if 0 then no error.
 * @return On success, returns the virtual address. On failure, returns `NULL` and `errno` is set.
 */
void* space_mapping_end(space_t* space, space_mapping_t* mapping, errno_t err);

/**
 * @brief Checks if a virtual memory region is fully mapped.
 *
 * @param space The target address space.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @return `true` if the entire region is mapped, `false` otherwise.
 */
bool space_is_mapped(space_t* space, const void* virtAddr, uint64_t length);

/** @} */
