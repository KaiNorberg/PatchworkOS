#pragma once

#include "sched/thread.h"
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
 * The Virtual Memory Manager (VMM) is responsible for allocating and mapping virtual memory.
 *
 * ## Address Space Layout
 *
 * The address space layout is split into several regions. For convenience, the regions are defined using page table
 * indices, as in the entire virtual address space is divided into 512 regions, each mapped by one entry in the top
 * level page table (PML4) with 256 entries for the lower half and 256 entries for the higher half. By doing this we can
 * very easily copy mappings between address spaces by just copying the relevant PML4 entries.
 *
 * First, at the very top, we have the kernel binary itself and all its data, code, bss, rodata, etc. This region uses
 * the the last index in the page table. This region will never be fully filled and the kernel itself is not guaranteed
 * to be loaded at the very start of this region, the exact address is decided by the `linker.lds` script. This section
 * is mapped identically for all processes.
 *
 * Secondly, we have the per-thread kernel stacks, one stack per thread. Each stack is allocated on demand and can grow
 * dynamically up to `CONFIG_MAX_KERNEL_STACK_PAGES` pages not including its guard page. This section takes up 2 indices
 * in the page table and will be process-specific as each process has its own threads and thus its own kernel stacks.
 *
 * Thirdly, we have the kernel heap, which is used for dynamic memory allocation in the kernel. The kernel heap starts
 * at `VMM_KERNEL_HEAP_MIN` and grows up towards `VMM_KERNEL_HEAP_MAX`. This section takes up 2 indices in the
 * page table and is mapped identically for all processes.
 *
 * Fourthly (is fourthly really a word?), we have the identity mapped physical memory. All physical memory will be
 * mapped here by simply taking the original physical address and adding `0xFFFF800000000000` to it. This means that the
 * physical address `0x123456` will be mapped to the virtual address `0xFFFF800000123456`. This section takes up all
 * remaining indices below the kernel heap to the end of the higher half and is mapped identically for all processes.
 *
 * Fithly, we have non-canonical memory, which is impossible to access and will trigger a general protection fault if
 * accessed. This section takes up the gap between the lower half and higher half of the address space.
 *
 * Finally, we have user space, which starts at `0x400000` (4MiB) and goes up to the top of the lower half. The first
 * 4MiB is left unmapped to catch null pointer dereferences. This section is different for each process.
 *
 * @{
 */

#define VMM_KERNEL_BINARY_MAX PML_HIGHER_HALF_END ///< The maximum address for the content of the kernel binary.
#define VMM_KERNEL_BINARY_MIN \
    PML_INDEX_TO_ADDR(PML_INDEX_AMOUNT - 1, PML4) ///< The minimum address for the content of the kernel binary.

#define VMM_KERNEL_STACKS_MAX VMM_KERNEL_BINARY_MIN                         ///< The maximum address for kernel stacks.
#define VMM_KERNEL_STACKS_MIN PML_INDEX_TO_ADDR(PML_INDEX_AMOUNT - 3, PML4) ///< The minimum address for kernel stacks.

#define VMM_KERNEL_HEAP_MAX VMM_KERNEL_STACKS_MIN                         ///< The maximum address for the kernel heap.
#define VMM_KERNEL_HEAP_MIN PML_INDEX_TO_ADDR(PML_INDEX_AMOUNT - 5, PML4) ///< The minimum address for the kernel heap.

#define VMM_IDENTITY_MAPPED_MAX VMM_KERNEL_HEAP_MIN   ///< The maximum address for the identity mapped physical memory.
#define VMM_IDENTITY_MAPPED_MIN PML_HIGHER_HALF_START ///< The minimum address for the identity mapped physical memory.

#define VMM_USER_SPACE_MAX PML_LOWER_HALF_END ///< The maximum address for user space.
#define VMM_USER_SPACE_MIN (0x400000)         ///< The minimum address for user space.

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
 * @brief Maps the lower half of the address space to the boot thread during kernel initialization.
 *
 * We still need to access bootloader data like the memory map while the kernel is initializing, so we keep the lower
 * half mapped until the kernel is fully initialized. After that we can unmap the lower half both from kernel space and
 * the boot thread's address space.
 *
 * The bootloades lower half mappings will be transfered to the kernel space mappings during boot so we just copy them
 * from there.
 *
 * @param bootThread The boot thread, which will have its address space modified.
 */
void vmm_map_bootloader_lower_half(thread_t* bootThread);

/**
 * @brief Unmaps the lower half of the address space after kernel initialization.
 *
 * Will unmap the lower half from both the kernel space and the boot thread's address space.
 *
 * After this is called the bootloaders lower half mappings will be destroyed and the kernel will only have its own
 * mappings.
 *
 * @param bootThread The boot thread, which will have its address space modified.
 */
void vmm_unmap_bootloader_lower_half(thread_t* bootThread);

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
