#pragma once

#include "space.h"

#include <boot/boot_info.h>
#include <common/paging_types.h>

#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Virtual Memory Manager (VMM).
 * @defgroup kernel_mem_vmm VMM
 * @ingroup kernel_mem
 *
 */

/**
 * @brief Initializes the Virtual Memory Manager.
 * @ingroup kernel_mem_vmm
 *
 * @param memory The memory information provided by the bootloader.
 * @param gop The graphics output protocol provided by the bootloader.
 * @param kernel The structure provided by the bootloader specifying for example the addresses of the kernel.
 */
void vmm_init(boot_memory_t* memory, boot_gop_t* gop, boot_kernel_t* kernel);

/**
 * @brief Unmaps the lower half of the address space after kernel initialization.
 * @ingroup kernel_mem_vmm
 * 
 */
void vmm_unmap_lower_half(void);

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
page_table_t* vmm_kernel_pml(void);

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
 * @brief Aligns a virtual address and length to page boundaries.
 * @ingroup kernel_mem_vmm
 *
 * @param virtAddr The virtual address to align.
 * @param length The length of the memory region.
 */
void vmm_align_region(void** virtAddr, uint64_t* length);

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
void* vmm_map(space_t* space, void* virtAddr, void* physAddr, uint64_t length, prot_t prot, space_callback_func_t func,
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
    space_callback_func_t func, void* private);
