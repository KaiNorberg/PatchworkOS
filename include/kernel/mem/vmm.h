#pragma once

#include <kernel/mem/paging_types.h>
#include <kernel/mem/space.h>
#include <kernel/sched/thread.h>

#include <boot/boot_info.h>

#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Virtual Memory Manager (VMM).
 * @defgroup kernel_mem_vmm VMM
 * @ingroup kernel_mem
 *
 * The Virtual Memory Manager (VMM) is responsible for allocating and mapping virtual memory.
 *
 * ## TLB Shootdowns
 *
 * When we change a mapping in a address space its possible that other CPUs have the same address space loaded and
 * have the old mappings in their "TLB", which is a hardware feature letting the CPUs cache page table entries. This
 * cache must be cleared when we change the mappings of a page table. This is called a TLB shootdown.
 *
 * Details can be found in `vmm_map()`, `vmm_unmap()` and `vmm_protect()`.
 *
 * ## Address Space Layout
 *
 * The address space layout is split into several regions. For convenience, the regions are defined using page table
 * indices, as in the entire virtual address space is divided into 512 regions, each mapped by one entry in the top
 * level page table (PML4) with 256 entries for the lower half and 256 entries for the higher half. By doing this we can
 * very easily copy mappings between address spaces by just copying the relevant PML4 entries.
 *
 * First, at the very top, we have the kernel binary itself and all its data, code, bss, rodata, etc. This region uses
 * the last index in the page table. This region will never be fully filled and the kernel itself is not guaranteed to
 * be loaded at the very start of this region, the exact address is decided by the `linker.lds` script. This section is
 * mapped identically for all processes.
 *
 * Secondly, we have the per-thread kernel stacks, one stack per thread. Each stack is allocated on demand and can grow
 * dynamically up to `CONFIG_MAX_KERNEL_STACK_PAGES` pages not including its guard page. This section takes up 2 indices
 * in the page table and will be process-specific as each process has its own threads and thus its own kernel stacks.
 *
 * Thirdly, we have the kernel heap, which is used for dynamic memory allocation in the kernel. The kernel heap starts
 * at `VMM_KERNEL_HEAP_MIN` and grows up towards `VMM_KERNEL_HEAP_MAX`. This section takes up 2 indices in the
 * page table and is mapped identically for all processes.
 *
 * Fourthly, we have the identity mapped physical memory. All physical memory will be
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
 * @brief TLB shootdown structure.
 * @struct vmm_shootdown_t
 *
 * Stored in a CPU's shootdown list and will be processed when it receives a `INTERRUPT_TLB_SHOOTDOWN` interrupt.
 */
typedef struct
{
    list_entry_t entry;
    space_t* space;
    void* virtAddr;
    uint64_t pageAmount;
} vmm_shootdown_t;

/**
 * @brief Maximum number of shootdown requests that can be queued per CPU.
 */
#define VMM_MAX_SHOOTDOWN_REQUESTS 16

/**
 * @brief Per-CPU VMM context.
 * @struct vmm_cpu_t
 */
typedef struct
{
    vmm_shootdown_t shootdowns[VMM_MAX_SHOOTDOWN_REQUESTS];
    uint8_t shootdownCount;
    lock_t lock;
    space_t* space; ///< Will only be accessed by the owner CPU, so no lock.
} vmm_cpu_t;

/**
 * @brief Flags for `vmm_alloc()`.
 * @enum vmm_alloc_flags_t
 */
typedef enum
{
    VMM_ALLOC_OVERWRITE = 0 << 0,      ///< If any page is already mapped, overwrite the mapping.
    VMM_ALLOC_FAIL_IF_MAPPED = 1 << 0, ///< If set and any page is already mapped, fail and set `errno` to `EEXIST`.
    VMM_ALLOC_ZERO = 1 << 1            ///< If set, atomically zero the allocated pages.
} vmm_alloc_flags_t;

/**
 * @brief Initializes the Virtual Memory Manager.
 */
void vmm_init(void);

/**
 * @brief Loads the kernel's address space into the current CPU.
 */
void vmm_kernel_space_load(void);

/**
 * @brief Retrieves the kernel's address space.
 *
 * @return Pointer to the kernel's address space.
 */
space_t* vmm_kernel_space_get(void);

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
 * The allocated memory will be backed by newly allocated physical memory pages and is not guaranteed to be zeroed.
 *
 * @see `vmm_map()` for details on TLB shootdowns.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param addr The output pointer to store the virtual address, the value it currently points to is used as the desired virtual address. If it points to `NULL`, the kernel chooses an address.
 * @param length The length of the virtual memory region to allocate, in bytes.
 * @param alignment The required alignment for the virtual memory region in bytes.
 * @param pmlFlags The page table flags for the mapping, will always include `PML_OWNED`, must have `PML_PRESENT` set.
 * @param allocFlags The allocation flags.
 * @return An appropriate status value.
 */
status_t vmm_alloc(space_t* space, void** addr, size_t length, size_t alignment, pml_flags_t pmlFlags,
    vmm_alloc_flags_t allocFlags);

/**
 * @brief Maps physical memory to virtual memory in a given address space.
 *
 * Will overwrite any existing mappings in the specified range.
 *
 * When mapping a page there is no need for a TLN shootdown as any previous access to that page will cause a non-present
 * page fault. However if the page is already mapped then it must first be unmapped as described in `vmm_unmap()`.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param addr The output pointer to store the virtual address, the value it currently points to is used as the desired virtual address. If it points to `NULL`, the kernel chooses an address.
 * @param phys The physical address to map from.
 * @param length The length of the memory region to map, in bytes.
 * @param flags The page table flags for the mapping, must have `PML_PRESENT` set.
 * @param func The callback function to call when the mapped memory is unmapped or the address space is freed. If
 * `NULL`, then no callback will be called.
 * @param data Private data to pass to the callback function.
 * @return An appropriate status value.
 */
status_t vmm_map(space_t* space, void** addr, phys_addr_t phys, size_t length, pml_flags_t flags,
    space_callback_func_t func, void* data);

/**
 * @brief Maps an array of physical pages to virtual memory in a given address space.
 *
 * Will overwrite any existing mappings in the specified range.
 *
 * @see `vmm_map()` for details on TLB shootdowns.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param virtAddr The output pointer to store the virtual address, the value it currently points to is used as the desired virtual address. If it points to `NULL`, the kernel chooses an address.
 * @param pfns An array of page frame numbers to map from.
 * @param amount The number of pages to map.
 * @param flags The page table flags for the mapping, must have `PML_PRESENT` set.
 * @param func The callback function to call when the mapped memory is unmapped or the address space is freed. If
 * `NULL`, then no callback will be called.
 * @param private Private data to pass to the callback function.
 * @return An appropriate status value.
 */
status_t vmm_map_pages(space_t* space, void** addr, pfn_t* pfns, size_t amount, pml_flags_t flags,
    space_callback_func_t func, void* data);

/**
 * @brief Unmaps virtual memory from a given address space.
 *
 * If the memory is already unmapped, this function will do nothing.
 *
 * When unmapping memory, there is a need for TLB shootdowns on all CPUs that have the address space loaded. To perform
 * the shootdown we first set all page entries for the region to be non-present, perform the shootdown, wait for
 * acknowledgements from all CPUs, and finally free any underlying physical memory if the `PML_OWNED` flag is set.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @return An appropriate status value.
 */
status_t vmm_unmap(space_t* space, void* virtAddr, size_t length);

/**
 * @brief Changes memory protection flags for a virtual memory region in a given address space.
 *
 * The memory region must be fully mapped, otherwise this function will fail.
 *
 * When changing memory protection flags, there is a need for TLB shootdowns on all CPUs that have the address space
 * loaded. To perform the shootdown we first update the page entries for the region, perform the shootdown, and wait for
 * acknowledgements from all CPUs and finally return.
 *
 * @param space The target address space, if `NULL`, the kernel space is used.
 * @param virtAddr The virtual address of the memory region.
 * @param length The length of the memory region, in bytes.
 * @param flags The new page table flags for the memory region, if `PML_PRESENT` is not set, the memory will be
 * unmapped.
 * @return An appropriate status value.
 */
status_t vmm_protect(space_t* space, void* virtAddr, size_t length, pml_flags_t flags);

/**
 * @brief Loads a virtual address space.
 *
 * Must be called with interrupts disabled.
 *
 * Will do nothing if the space is already loaded.
 *
 * @param space The address space to load.
 */
void vmm_load(space_t* space);

/**
 * @brief Performs a TLB shootdown for a region of the address space, and wait for acknowledgements.
 *
 * Must be called between `space_mapping_start()` and `space_mapping_end()`.
 *
 * This will cause all CPUs that have the address space loaded to invalidate their TLB entries for the specified region.
 *
 * Will not affect the current CPU's TLB, that is handled by the `page_table_t` directly when modifying page table
 * entries.
 *
 * @todo Currently this does a busy wait for acknowledgements. Use a wait queue?
 *
 * @param space The target address space.
 * @param virtAddr The starting virtual address of the region.
 * @param pageAmount The number of pages in the region.
 */
void vmm_tlb_shootdown(space_t* space, void* virtAddr, size_t pageAmount);

/** @} */
