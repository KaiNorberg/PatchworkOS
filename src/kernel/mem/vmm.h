#pragma once

#include "space.h"

#include <boot/boot_info.h>
#include <common/paging_types.h>

#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Virtual Memory Manager (VMM).
 * @defgroup kernel_mem_vmm VMM
 *
 */

/**
 * @brief Check if an address is page aligned.
 *
 * @param addr The address to check.
 * @return true if the address is page aligned, false otherwise.
 */
#define VMM_IS_PAGE_ALIGNED(addr) (((uintptr_t)(addr) & (PAGE_SIZE - 1)) == 0)

/**
 * @brief Initializes the Virtual Memory Manager.
 *
 * @param memory The memory information provided by the bootloader.
 * @param gop The graphics output protocol provided by the bootloader.
 * @param kernel The structure provided by the bootloader specifying for example the addresses of the kernel.
 */
void vmm_init(const boot_memory_t* memory, const boot_gop_t* gop, const boot_kernel_t* kernel);

/**
 * @brief Initializes VMM for a CPU.
 *
 * The `vmm_cpu_init()` function performs per-CPU VMM initialization for the currently running CPU, for example enabling
 * global pages.
 *
 */
void vmm_cpu_init(void);

/**
 * @brief Unmaps the lower half of the address space after kernel initialization.
 *
 * We still need to access bootloader data like the memory map while the kernel is initializing, so we keep the lower
 * half mapped until the kernel is fully initialized. After that we can unmap the lower half both from kernel space and
 * the boot thread's address space.
 *
 * @param bootThread The boot thread, which will have its address space modified.
 */
void vmm_unmap_lower_half(thread_t* bootThread);

/**
 * @brief Retrieves the kernel's address space.
 *
 * @return Pointer to the kernel's address space.
 */
space_t* vmm_get_kernel_space(void);

/**
 * @brief Converts the user space memory protection flags to page table entry flags.
 *
 * @param prot The memory protection flags.
 * @return The corresponding page table entry flags.
 */
pml_flags_t vmm_prot_to_flags(prot_t prot);

/**
 * @brief Aligns a virtual address and length to page boundaries.
 *
 * @param virtAddr The virtual address to align.
 * @param length The length of the memory region.
 */
void vmm_align_region(void** virtAddr, uint64_t* length);

/**
 * @brief Allocates and maps virtual memory in a given address space.
 *
 * Will overwrite any existing mappings in the specified range.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param virtAddr The desired virtual address. If `NULL`, the kernel chooses an available address.
 * @param length The length of the virtual memory region to allocate, in bytes.
 * @param flags The page table flags for the mapping, will always include `PML_OWNED`, must have `PML_PRESENT` set.
 * @return On success, the virtual address. On failure, returns `NULL` and `errno` is set.
 */
void* vmm_alloc(space_t* space, void* virtAddr, uint64_t length, pml_flags_t flags);

/**
 * @brief Maps physical memory to virtual memory in a given address space.
 *
 * Will overwrite any existing mappings in the specified range.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param virtAddr The desired virtual address to map to, if `NULL`, the kernel chooses an available address.
 * @param physAddr The physical address to map from. Must not be `NULL`.
 * @param length The length of the memory region to map, in bytes.
 * @param flags The page table flags for the mapping, must have `PML_PRESENT` set.
 * @param func The callback function to call when the mapped memory is unmapped or the address space is freed. If
 * `NULL`, then no callback will be called.
 * @param private Private data to pass to the callback function.
 * @return On success, the virtual address. On failure, returns `NULL` and `errno` is set.
 */
void* vmm_map(space_t* space, void* virtAddr, void* physAddr, uint64_t length, pml_flags_t flags,
    space_callback_func_t func, void* private);

/**
 * @brief Maps an array of physical pages to virtual memory in a given address space.
 *
 * Will overwrite any existing mappings in the specified range.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param virtAddr The desired virtual address to map to, if `NULL`, the kernel chooses an available address.
 * @param pages An array of physical page addresses to map.
 * @param pageAmount The number of physical pages in the `pages` array, must not be zero.
 * @param flags The page table flags for the mapping, must have `PML_PRESENT` set.
 * @param func The callback function to call when the mapped memory is unmapped or the address space is freed. If
 * `NULL`, then no callback will be called.
 * @param private Private data to pass to the callback function.
 * @return On success, the virtual address. On failure, returns `NULL` and `errno` is set.
 */
void* vmm_map_pages(space_t* space, void* virtAddr, void** pages, uint64_t pageAmount, pml_flags_t flags,
    space_callback_func_t func, void* private);

/**
 * @brief Unmaps virtual memory from a given address space.
 *
 * If the memory is already unmapped, this function will do nothing.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @return On success, returns 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t vmm_unmap(space_t* space, void* virtAddr, uint64_t length);

/**
 * @brief Changes memory protection flags for a virtual memory region in a given address space.
 *
 * The memory region must be fully mapped, otherwise this function will fail.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @param flags The new page table flags for the memory region, if `PML_PRESENT` is not set, the memory will be
 * unmapped.
 * @return On success, returns 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t vmm_protect(space_t* space, void* virtAddr, uint64_t length, pml_flags_t flags);

/** @} */
