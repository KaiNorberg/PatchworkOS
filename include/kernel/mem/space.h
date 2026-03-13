#pragma once

#include <kernel/cpu/stack_pointer.h>
#include <kernel/fs/path.h>
#include <kernel/mem/paging_types.h>
#include <kernel/sync/lock.h>

#include <boot/boot_info.h>

#include <sys/bitmap.h>
#include <sys/list.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/status.h>

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
    page_table_t pageTable;      ///< The page table associated with the address space.
    uintptr_t startAddress;      ///< The start address for allocations in this address space.
    uintptr_t endAddress;        ///< The end address for allocations in this address space.
    uintptr_t freeAddress;       ///< The next available free virtual address in this address space.
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
 * @return An appropriate status value.
 */
status_t space_init(space_t* space, uintptr_t startAddress, uintptr_t endAddress, space_flags_t flags);

/**
 * @brief Deinitializes a virtual address space.

 * @param space The address space to deinitialize.
 */
void space_deinit(space_t* space);

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
 * @return `true` if the region is within the allowed address range, `false` otherwise.
 */
bool space_check_access(space_t* space, const void* addr, size_t length);

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
 * @return The number of user pages.
 */
uint64_t space_user_page_count(space_t* space);

/**
 * @brief Translate a virtual address to a physical address in the address space.
 *
 * @param out Output pointer for the physical address.
 * @param space The target address space.
 * @param virtAddr The virtual address to translate.
 * @return An appropriate status value.
 */
status_t space_virt_to_phys(phys_addr_t* out, space_t* space, const void* virtAddr);

/**
 * @brief Copy memory from an address space.
 *
 * @param space The address space to read from.
 * @param dest The destination buffer.
 * @param base The virtual address in the address space to read from.
 * @param size The number of bytes to read.
 * @return An appropriate status value.
 */
status_t space_copy_out(space_t* space, void* dest, const void* src, size_t size);

/**
 * @brief Copy memory to an address space.
 *
 * @param space The address space to write to.
 * @param dest The virtual address in the address space to write to.
 * @param src The source buffer.
 * @param size The number of bytes to write.
 * @return An appropriate status value.
 */
status_t space_copy_in(space_t* space, void* dest, const void* src, size_t size);

/** @} */
