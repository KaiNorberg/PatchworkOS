#pragma once

#include "defs.h"

#include <bootloader/boot_info.h>

#include <sys/proc.h>

/**
 * @brief Physical Memory Manager (PMM).
 * @defgroup kernel_mem_pmm PMM
 * @ingroup kernel_mem
 *
 * The Physical Memory Manager (PMM) is responsible for managing physical memory pages. It uses a free stack for allocating single pages in constant-time and for more specialized allocations for instance requiring a specific address range or alignment a bitmap allocator is used. The bitmap allocator should only be used when no other option is available.
 * 
 * All physical memory is identity mapped to the begining of the higher half of the address space. This means that for example `NULL` is always a invalid address and that the PMM returns addresses in the higher half.
 * 
 * The free stack provides some advnatages over for instance a free list, mainly due to cache improvements. 
 * 
 */

/**
 * @brief The maximum physical address managed by the bitmap.
 *
 * Physical addresses below this value are managed by a bitmap, while addresses above it are managed by a stack.
 */
#define PMM_BITMAP_MAX_ADDR (0x4000000ULL)

/**
 * @brief Structure for a page buffer in the PMM stack.
 * @ingroup kernel_mem_pmm
 * @struct page_buffer
 *
 * The `page_buffer_t` structure is stored in free pages and keeps track of pages that are currently freed.
 */
typedef struct page_buffer
{
    /**
     * @brief Pointer to the previous page buffer in the stack.
     */
    struct page_buffer* prev;
    /**
     * @brief Flexible array member to store free physical pages.
     */
    void* pages[];
} page_buffer_t;

/**
 * @brief The maximum number of pages that can be stored in a `page_buffer_t`.
 */
#define PMM_BUFFER_MAX ((PAGE_SIZE - sizeof(page_buffer_t)) / sizeof(void*))

/**
 * @brief PMM stack structure for managing higher physical memory.
 * @ingroup kernel_mem_pmm
 * @struct pmm_stack_t
 */
typedef struct
{
    /**
     * @brief Pointer to the last page buffer in the stack.
     */
    page_buffer_t* last;
    /**
     * @brief Current index within the `pages` array of the `last` page buffer.
     */
    uint64_t index;
} pmm_stack_t;

/**
 * @brief The number of pages that are be managed by the PMM bitmap.
 */
#define PMM_BITMAP_MAX (PMM_BITMAP_MAX_ADDR / PAGE_SIZE)

/**
 * @brief Initializes the Physical Memory Manager.
 * @ingroup kernel_mem_pmm
 *
 * @param memoryMap The EFI memory map provided by the bootloader.
 */
void pmm_init(efi_mem_map_t* memoryMap);

/**
 * @brief Allocates a single physical page.
 * @ingroup kernel_mem_pmm
 *
 * @return On success, returns the higher half physical address of the allocated page. On failure, returns `NULL.
 */
void* pmm_alloc(void);

/**
 * @brief Allocates a contiguous region of physical pages managed by the bitmap.
 * @ingroup kernel_mem_pmm
 *
 * The `pmm_alloc_bitmap` function allocates a contiguous block of `count` physical pages from the memory region
 * managed by the bitmap (i.e., below `PMM_BITMAP_MAX_ADDR`). It also enforces a maximum
 * address and alignment for the allocation.
 *
 * @param count The number of contiguous pages to allocate.
 * @param maxAddr The maximum physical address (exclusive) for the allocation.
 * @param alignment The required alignment for the allocated region, in bytes.
 * @return On success, returns the higher half physical address of the allocated region. On failure, returns `NULL`.
 */
void* pmm_alloc_bitmap(uint64_t count, uintptr_t maxAddr, uint64_t alignment);

/**
 * @brief Frees a single physical page.
 * @ingroup kernel_mem_pmm
 *
 * The `pmm_free` function frees a page returning ownership of it to the PMM, the PMM will determine based of the address if its owned by the bitmap or the stack allocator.
 *
 * @param address The higher half physical address of the page to free.
 */
void pmm_free(void* address);

/**
 * @brief Frees a contiguous region of physical pages.
 * @ingroup kernel_mem_pmm
 * 
 * The `pmm_free` function frees a contiguous block of `count` physical pages, returning ownership of them to the PMM, the PMM will determine based of the address if its owned by the bitmap or the stack allocator.
 * 
 * @param address The higher half physical address of the first page in the region to free.
 * @param count The number of pages to free.
 */
void pmm_free_pages(void* address, uint64_t count);

/**
 * @brief Retrieves the total amount of physical memory managed by the PMM.
 * @ingroup kernel_mem_pmm
 *
 * @return The total amount of physical memory in pages.
 */
uint64_t pmm_total_amount(void);

/**
 * @brief Retrieves the amount of free physical memory.
 * @ingroup kernel_mem_pmm
 *
 * @return The amount of currently free physical memory in pages.
 */
uint64_t pmm_free_amount(void);

/**
 * @brief Retrieves the amount of reserved physical memory.
 * @ingroup kernel_mem_pmm
 *
 * Reserved memory includes memory that is not available for allocation (e.g., kernel code, hardware regions).
 *
 * @return The amount of reserved physical memory in pages.
 */
uint64_t pmm_reserved_amount(void);
