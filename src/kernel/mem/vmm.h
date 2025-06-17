#pragma once

#include "defs.h"
#include "pml.h"
#include "sync/lock.h"
#include "utils/bitmap.h"

#include <bootloader/boot_info.h>

#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Virtual Memory Manager (VMM).
 * @defgroup kernel_mem_vmm Virtual Memory Manager
 * @ingroup kernel_mem
 *
 * The virtual memory manager in patchwork is intended to run in constant-time, for more information refer to the
 * "*Mostly* Constant-Time Memory Management" section of the README, this file will assume you have read it to avoid
 * duplicate information.
 *
 */

/**
 * @brief Kernel pml flags.
 * @ingroup kernel_mem_vmm
 *
 * The `VMM_KERNEL_PML_FLAGS` constant defines the flags that all pages allocated in kernel space will have.
 *
 */
#define VMM_KERNEL_PML_FLAGS (PML_GLOBAL)

/**
 * @brief VMM callback function.
 * @ingroup kernel_mem_vmm
 */
typedef void (*vmm_callback_func_t)(void* private);

/**
 * @brief VMM callback structure.
 * @ingroup kernel_mem_vmm
 * @struct vmm_callback_t
 */
typedef struct
{
    /**
     * @brief The callback function to be invoked.
     */
    vmm_callback_func_t func;
    /**
     * @brief Private data to be passed to the callback function.
     */
    void* private;
    /**
     * @brief The amount of pages associated with this callback.
     */
    uint64_t pageAmount;
} vmm_callback_t;

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
    pml_t* pml;
    /**
     * @brief The next available free virtual address in this address space.
     */
    uintptr_t freeAddress;
    /**
     * @brief Array of VMM callbacks for this address space, indexed by the callback ID.
     */
    vmm_callback_t callbacks[PML_MAX_CALLBACK];
    /**
     * @brief Bitmap to track available callback IDs.
     */
    bitmap_t callbackBitmap;
    /**
     * @brief Buffer for the callback bitmap, see `bitmap_t` for more info.
     */
    uint64_t bitmapBuffer[BITMAP_BITS_TO_QWORDS(PML_MAX_CALLBACK)];
    /**
     * @brief Lock to protect this structure.
     */
    lock_t lock;
} space_t;

/**
 * @brief Initializes a virtual address space.
 * @ingroup kernel_mem_vmm
 *
 * @param space The address space to initialize.
 */
void space_init(space_t* space);

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
 * @param space The address space to load, if `NULL` loads kernel pml.
 */
void space_load(space_t* space);

/**
 * @brief Initializes the Virtual Memory Manager.
 * @ingroup kernel_mem_vmm
 *
 * @param memoryMap The EFI memory map provided by the bootloader.
 * @param kernel The structure provided by the bootloader specifying for example the addresses of the kernel.
 * @param gopBuffer The gop framebuffer provided by the bootloader, which will need to be remapped when the kernel
 * isloaded, should probably be replaced.
 */
void vmm_init(efi_mem_map_t* memoryMap, boot_kernel_t* kernel, gop_buffer_t* gopBuffer);

/**
 * @brief Initializes VMM for a CPU.
 * @ingroup kernel_mem_vmm
 *
 * The `vmm_cpu_init()` function performs per-CPU VMM initialization for the currently running CPU, for example enabling
 * global pages.
 *
 */
void vmm_cpu_init(void);

/**
 * @brief Retrieves the kernel's page tables.
 * @ingroup kernel_mem_vmm
 *
 * @return The kernels page tables.
 */
pml_t* vmm_kernel_pml(void);

/**
 * @brief Maps physical memory to kernel space virtual memory.
 * @ingroup kernel_mem_vmm
 *
 * @param virtAddr The desired virtual address to map to, must be page aligned, if `NULL` then we use the virtual
 * address `physAddr` + `PML_HIGHER_HALF_START`.
 * @param physAddr The physical address to map from, must be page aligned, if `NULL` then allocate pages using the pmm
 * and OR page table flags with PML_OWNED.
 * @param pageAmount The amount of pages to map, must not be 0.
 * @param flags The page table flags of the mapped pages, will OR with `VMM_KERNEL_PML_FLAGS`.
 * @return On success, returns the virtual address where the memory was mapped. On failure, returns `NULL`.
 */
void* vmm_kernel_map(void* virtAddr, void* physAddr, uint64_t pageAmount, pml_flags_t flags);

/**
 * @brief Unmaps virtual memory from kernel space.
 * @ingroup kernel_mem_vmm
 *
 * @param virtAddr The virtual address of the memory region.
 * @param pageAmount The number of pages in the memory region.
 */
void vmm_kernel_unmap(void* virtAddr, uint64_t pageAmount);

/**
 * @brief Allocates and maps virtual memory in a given address space.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param virtAddr The desired virtual address. If `NULL`, the kernel chooses an available address.
 * @param length The length of the virtual memory region to allocate, in bytes.
 * @param prot The memory protection flags (e.g., `PROT_READ`, `PROT_WRITE`).
 * @return On success, returns the allocated virtual address. On failure, returns `NULL`.
 */
void* vmm_alloc(space_t* space, void* virtAddr, uint64_t length, prot_t prot);

/**
 * @brief Maps physical memory to virtual memory in a given address space.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param virtAddr The desired virtual address to map to.
 * @param physAddr The physical address to map from.
 * @param length The length of the memory region to map, in bytes.
 * @param prot The memory protection flags.
 * @param func The callback function to call when the mapped memory is unmapped or the address space is freed. If
 * `NULL`, then no callback will be called.
 * @param private Private data to pass to the callback function.
 * @return On success, returns the virtual address where the memory was mapped. On failure, returns `NULL`.
 */
void* vmm_map(space_t* space, void* virtAddr, void* physAddr, uint64_t length, prot_t prot, vmm_callback_func_t func,
    void* private);

/**
 * @brief Maps a list of physical pages to virtual memory in a given address space.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param virtAddr The desired virtual address to map to.
 * @param pages An array of physical page addresses to map.
 * @param pageAmount The number of physical pages in the `pages` array.
 * @param prot The memory protection flags.
 * @param func The callback function to call when the mapped memory is unmapped or the address space is freed. If
 * `NULL`, then no callback will be called.
 * @param private Private data to pass to the callback function.
 * @return On success, returns the virtual address where the memory was mapped. On failure, returns `NULL`.
 */
void* vmm_map_pages(space_t* space, void* virtAddr, void** pages, uint64_t pageAmount, prot_t prot,
    vmm_callback_func_t func, void* private);

/**
 * @brief Unmaps virtual memory from a given address space.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @return On success, returns 0. On failure, returns `ERR`.
 */
uint64_t vmm_unmap(space_t* space, void* virtAddr, uint64_t length);

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
uint64_t vmm_protect(space_t* space, void* virtAddr, uint64_t length, prot_t prot);

/**
 * @brief Checks if a virtual memory region is fully mapped.
 * @ingroup kernel_mem_vmm
 *
 * @param space The target address space.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @return `true` if the entire region is mapped, `false` otherwise.
 */
bool vmm_mapped(space_t* space, const void* virtAddr, uint64_t length);
