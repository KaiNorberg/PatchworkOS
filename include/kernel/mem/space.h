#pragma once

#include <kernel/cpu/stack_pointer.h>
#include <kernel/fs/path.h>
#include <kernel/mem/paging_types.h>
#include <kernel/sync/lock.h>

#include <boot/boot_info.h>

#include <sys/bitmap.h>
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
typedef void (*space_callback_func_t)(void* data);

/**
 * @brief Space callback structure.
 * @struct vmm_callback_t
 */
typedef struct
{
    space_callback_func_t func;
    void* data;
    uint64_t pageAmount;
} space_callback_t;

/**
 * @brief Pinned page structure.
 * @struct space_pinned_page_t
 *
 * Stored in the `pinnedPages` map in `space_t`.
 */
typedef struct
{
    map_entry_t mapEntry;
    uint64_t pinCount; ///< The number of times this page is pinned, will be unpinned when it reaches 0.
} space_pinned_page_t;

/**
 * @brief Virtual address space structure.
 * @struct space_t
 *
 * The `space_t` structure represents a virtual address space.
 *
 * Note that the actual pin depth, if its is greater than 1, is tracked in the `pinnedPages` map, the page table only
 * tracks if a page is pinned or not for faster access and to avoid having to access the map even when just pinning a
 * page once.
 */
typedef struct space
{
    page_table_t pageTable; ///< The page table associated with the address space.
    map_t pinnedPages;      ///< Map of pages with a pin depth greater than 1.
    uintptr_t startAddress; ///< The start address for allocations in this address space.
    uintptr_t endAddress;   ///< The end address for allocations in this address space.
    uintptr_t freeAddress;  ///< The next available free virtual address in this address space.
    space_flags_t flags;
    /**
     * Array of callbacks for this address space, indexed by the callback ID.
     *
     * Lazily initialized to a size equal to the largest used callback ID.
     */
    space_callback_t* callbacks;
    uint64_t callbacksLength;                        ///< Length of the `callbacks` array.
    BITMAP_DEFINE(callbackBitmap, PML_MAX_CALLBACK); ///< Bitmap to track available callback IDs.
    BITMAP_DEFINE(cpus, CPU_MAX);                    ///< Bitmap to track which CPUs are using this space.
    atomic_uint16_t shootdownAcks;
    lock_t lock;
} space_t;

/**
 * @brief The maximum time to wait for the acknowledgements from other CPU's before panicking.
 */
#define SPACE_TLB_SHOOTDOWN_TIMEOUT (CLOCKS_PER_SEC)

/**
 * @brief Initializes a virtual address space.
 *
 * @param space The address space to initialize.
 * @param startAddress The starting address for allocations in this address space.
 * @param endAddress The ending address for allocations in this address space.
 * @param flags Flags to control the initialization behavior.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOMEM`: Not enough memory to initialize the address space.
 */
uint64_t space_init(space_t* space, uintptr_t startAddress, uintptr_t endAddress, space_flags_t flags);

/**
 * @brief Deinitializes a virtual address space.

 * @param space The address space to deinitialize.
 */
void space_deinit(space_t* space);

/**
 * @brief Pins pages within a region of the address space.
 *
 * Used to prevent TOCTOU attacks, where a system call provides some user space region, the kernel then checks that its
 * mapped and after that check a seperate thread in the user space process unmaps or modifies that regions mappings
 * while the kernel is still using it.
 *
 * Our solution is to pin any user space pages that are accessed or modified during the syscall, meaning that a special
 * flag is set in the address spaces page tables that prevent those pages from being unmapped or modified until they are
 * unpinned which happens when the syscall is finished in `space_unpin()`.
 *
 * If the region is not fully mapped, or the region is not within the spaces `startAddress` and `endAddress` range, the
 * function will fail.
 *
 * If any page in the region is already at its maximum pin depth, the calling thread will block until the page is
 * unpinned by another thread.
 *
 * If a user stack is provided and the region to pin is both unmapped and within the stack region, memory will be
 * allocated and mapped to the relevant region in the user stack. This is needed as its possible for a user space
 * process to pass an address to a system call that is in its user stack but not yet mapped. For example, it could
 * create a big buffer on its stack then pass it to a syscall without first accessing it, meaning no page fault would
 * have occurred to map the pages.
 *
 * @param space The target address space.
 * @param address The address to pin, can be `NULL` if length is 0.
 * @param length The length of the region pointed to by `address`, in bytes.
 * @param userStack Pointer to the user stack of the calling thread, can be `NULL, see above.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EOVERFLOW`: Address overflow.
 * - `EFAULT`: The region is not fully mapped or within the provided user stack.
 * - `ENOMEM`: Not enough memory.
 */
uint64_t space_pin(space_t* space, const void* address, size_t length, stack_pointer_t* userStack);

/**
 * @brief Pins a region of memory terminated by a terminator value.
 *
 * Pins pages in the address space starting from `address` up to `maxSize` bytes or until the specified
 * terminator is found.
 *
 * Used for null-terminated strings or other buffers that have a specific terminator.
 *
 * @param space The target address space.
 * @param address The starting address of the region to pin.
 * @param terminator The terminator value to search for.
 * @param objectSize The size of each object to compare against the terminator, in bytes.
 * @param maxCount The maximum number of objects to scan before failing.
 * @param userStack Pointer to the user stack of the calling thread, can be `NULL`, see `space_pin()`.
 * @return On success, the number of bytes pinned, not including the terminator. On failure, `ERR` and `errno` is set
 * to:
 * - `EINVAL`: Invalid parameters.
 * - `EOVERFLOW`: Address overflow.
 * - `EFAULT`: The region is not fully mapped or within the provided user stack.
 * - `ENOMEM`: Not enough memory.
 */
uint64_t space_pin_terminated(space_t* space, const void* address, const void* terminator, size_t objectSize,
    size_t maxCount, stack_pointer_t* userStack);

/**
 * @brief Unpins pages in a region previously pinned with `space_pin()` or `space_pin_string()`.
 *
 * Will wake up any threads waiting to pin the same pages.
 *
 * @param space The target address space.
 * @param address The address of the region to unpin, can be `NULL` if length is 0.
 * @param length The length of the region pointed to by `address`, in bytes.
 */
void space_unpin(space_t* space, const void* address, size_t length);

/**
 * @brief Checks if a virtual memory region is within the allowed address range of the space.
 *
 * Checks that the given memory region is within the `startAddress` and `endAddress` range of the space, really only
 * used in system calls that might access unmapped user space memory for example `mmap()`, in such cases we dont want to
 * pin the "buffer" since we expect that it is not yet mapped.
 *
 * @param space The target address space.
 * @param addr The starting address of the memory region, can be `NULL` if length is 0.
 * @param length The length of the memory region, in bytes.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EOVERFLOW`: Address overflow.
 * - `EFAULT`: The region is outside the allowed address range.
 */
uint64_t space_check_access(space_t* space, const void* addr, size_t length);

/**
 * @brief Helper structure for managing address space mappings.
 * @struct space_mapping_t
 */
typedef struct
{
    void* virtAddr;
    phys_addr_t physAddr;
    size_t pageAmount;
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
 * @note Handling page faults to grow stacks requires mapping memory, this means that if we were to run out of memory
 * while executing this function, it could lead to a deadlock. To avoid this, this function will call
 * `stack_pointer_poke()` to ensure that sufficient stack space is available.
 *
 * @param space The target address space.
 * @param mapping Will be filled with parsed information about the mapping.
 * @param virtAddr The virtual address the mapping will apply to. Can be `NULL` to let the kernel choose an address.
 * @param physAddr The physical address to map from. Can be `PHYS_ADDR_INVALID`.
 * @param length The length of the virtual memory region to modify, in bytes.
 * @param alignment The required alignment for the virtual memory region in bytes.
 * @param flags The page table flags for the mapping.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EOVERFLOW`: Address overflow.
 * - `EFAULT`: The addresses are outside the allowed range.
 * - `ENOMEM`: Not enough memory.
 */
uint64_t space_mapping_start(space_t* space, space_mapping_t* mapping, void* virtAddr, phys_addr_t physAddr, size_t length,
    size_t alignment, pml_flags_t flags);

/**
 * @brief Allocate a callback.
 *
 * Must be called between `space_mapping_start()` and `space_mapping_end()`.
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
pml_callback_id_t space_alloc_callback(space_t* space, size_t pageAmount, space_callback_func_t func, void* data);

/**
 * @brief Free a callback.
 *
 * Must be called between `space_mapping_start()` and `space_mapping_end()`.
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
 * Must be called after `space_mapping_start()`.
 *
 * @param space The target address space.
 * @param mapping The parsed information about the mapping.
 * @param err The error code, if 0 then no error.
 * @return If `err` is `EOK`, returns the virtual address of the mapping. If `err` is not `EOK`, returns `NULL` and
 * `errno` is set to `err`.
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
bool space_is_mapped(space_t* space, const void* virtAddr, size_t length);

/**
 * @brief Get the number of user pages allocated in the address space.
 *
 * Will count the number of pages with the `PML_OWNED` flag set in user space.
 *
 * @param space The target address space.
 * @return The number of user pages mapped.
 */
uint64_t space_user_page_count(space_t* space);

/** @} */
