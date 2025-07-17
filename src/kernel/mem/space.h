#pragma once

#include "sync/rwmutex.h"
#include "utils/bitmap.h"

#include <boot/boot_info.h>
#include <common/paging_types.h>

#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Space callback function.
 * @ingroup kernel_mem_vmm
 */
typedef void (*space_callback_func_t)(void* private);

/**
 * @brief Space callback structure.
 * @ingroup kernel_mem_vmm
 * @struct vmm_callback_t
 */
typedef struct
{
    /**
     * @brief The callback function to be invoked.
     */
    space_callback_func_t func;
    /**
     * @brief Private data to be passed to the callback function.
     */
    void* private;
    /**
     * @brief The amount of pages associated with this callback, when this reaches zero the func is called.
     */
    uint64_t pageAmount;
} space_callback_t;

/**
 * @brief Virtual address space structure.
 * @ingroup kernel_mem_vmm
 * @struct space_t
 *
 * The `space_t` structure represents a virtual address space.
 *
 */
typedef struct
{
    /**
     * @brief The page table associated with the address space.
     */
    page_table_t pageTable;
    /**
     * @brief The next available free virtual address in this address space.
     */
    uintptr_t freeAddress;
    /**
     * @brief Array of callbacks for this address space, indexed by the callback ID.
     */
    space_callback_t callbacks[PML_MAX_CALLBACK];
    /**
     * @brief Bitmap to track available callback IDs.
     */
    bitmap_t callbackBitmap;
    /**
     * @brief Buffer for the callback bitmap, see `bitmap_t` for more info.
     */
    uint64_t bitmapBuffer[BITMAP_BITS_TO_QWORDS(PML_MAX_CALLBACK)];
    /**
     * @brief Mutex to protect this structure.
     */
    rwmutex_t mutex;
} space_t;

/**
 * @brief Initializes a virtual address space.
 * @ingroup kernel_mem_vmm
 *
 * @param space The address space to initialize.
 * @return ERR if an error occurred, 0 otherwise.
 */
uint64_t space_init(space_t* space);

/**
 * @brief Deinitializes a virtual address space.
 * @ingroup kernel_mem_vmm

 * @param space The address space to deinitialize.
 */
void space_deinit(space_t* space);

/**
 * @brief Loads a virtual address space.
 * @ingroup kernel_mem_vmm
 *
 * @param space The address space to load.
 */
void space_load(space_t* space);

typedef struct
{
    void* virtAddr;
    void* physAddr;
    uint64_t pageAmount;
    pml_flags_t flags;
} space_mapping_t;

/**
 * @brief Prepare for changes to the address space mappings.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param mapping Will be filled with parsed information about the mapping.
 * @return On success, returns 0. On failure, returns `ERR`.
 */
uint64_t space_mapping_start(space_t* space, space_mapping_t* mapping, void* virtAddr, void* physAddr,
    uint64_t length, prot_t prot);

/**
 * @brief Allocate a callback.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param pageAmount The number of pages the callback is responsible for.
 * @param func The callback function.
 * @param private Private data to pass to the callback function.
 * @return On success, returns the callback ID. On failure, returns `PML_MAX_CALLBACK`.
 */
pml_callback_id_t space_alloc_callback(space_t* space, uint64_t pageAmount, space_callback_func_t func,
    void* private);

/**
 * @brief Free a callback.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param callbackId The callback ID to free.
 */
void space_free_callback(space_t* space, pml_callback_id_t callbackId);

/**
 * @brief Performs cleanup after changes to the address space mappings.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param mapping The parsed information about the mapping.
 * @param err The error code, if 0 then no error.
 * @return On success, returns 0. On failure, returns `ERR`.
 */
void* space_mapping_end(space_t* space, space_mapping_t* mapping, errno_t err);

/**
 * @brief Unmaps virtual memory from a given address space.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @return On success, returns 0. On failure, returns `ERR`.
 */
uint64_t space_unmap(space_t* space, void* virtAddr, uint64_t length);

/**
 * @brief Changes memory protection flags for a virtual memory region.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @param prot The new memory protection flags.
 * @return On success, returns 0. On failure, returns `ERR`.
 */
uint64_t space_protect(space_t* space, void* virtAddr, uint64_t length, prot_t prot);

/**
 * @brief Checks if a virtual memory region is fully mapped.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @return `true` if the entire region is mapped, `false` otherwise.
 */
bool space_is_mapped(space_t* space, const void* virtAddr, uint64_t length);
