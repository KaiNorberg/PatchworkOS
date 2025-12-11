#pragma once

#include <boot/boot_info.h>

/**
 * @brief Physical Memory Manager (PMM).
 * @defgroup kernel_mem_pmm PMM
 * @ingroup kernel_mem
 *
 * The Physical Memory Manager (PMM) is responsible for managing physical memory pages. It uses a free stack for
 * allocating single pages in constant-time, and for more specialized allocations (requiring a specific address range
 * or alignment) a bitmap allocator is used. The bitmap allocator should only be used when no other option is available.
 *
 * All physical memory is identity mapped to the beginning of the higher half of the address space. This means that for
 * example `NULL` is always an invalid address and that the PMM returns addresses in the higher half.
 *
 * The free stack provides some advantages over for instance a free list, mainly due to cache improvements.
 *
 * @{
 */

/**
 * @brief Initializes the Physical Memory Manager.
 */
void pmm_init(void);

/**
 * @brief Allocates a single physical page.
 *
 * The returned page will not be zeroed.
 *
 * @return On success, returns the higher half physical address of the allocated page. On failure, returns `NULL`.
 */
void* pmm_alloc(void);

/**
 * @brief Allocates multiple physical pages.
 *
 * The `pmm_alloc_pages` function allocates `count` non-contiguous physical pages from the free stack, by using this
 * function its possible to avoid holding the lock for each page allocation, improving performance when allocating many
 * pages at once.
 *
 * The returned pages will not be zeroed.
 *
 * @param addresses An array where the higher half physical addresses of the allocated pages will be stored.
 * @param count The number of pages to allocate.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t pmm_alloc_pages(void** addresses, uint64_t count);

/**
 * @brief Allocates a contiguous region of physical pages managed by the bitmap.
 *
 * The `pmm_alloc_bitmap` function allocates a contiguous block of `count` physical pages from the memory region
 * managed by the bitmap. It also enforces a maximum address and alignment for the allocation.
 *
 * The returned pages will not be zeroed.
 *
 * @param count The number of contiguous pages to allocate.
 * @param maxAddr The maximum physical address (exclusive) for the allocation.
 * @param alignment The required alignment for the allocated region, in bytes.
 * @return On success, returns the higher half physical address of the allocated region. On failure, returns `NULL`.
 */
void* pmm_alloc_bitmap(uint64_t count, uintptr_t maxAddr, uint64_t alignment);

/**
 * @brief Frees a single physical page.
 *
 * The `pmm_free` function frees a page returning ownership of it to the PMM. The PMM will determine based on the
 * address if it's owned by the bitmap or the free stack.
 *
 * @param address The higher half physical address of the page to free.
 */
void pmm_free(void* address);

/**
 * @brief Frees multiple physical pages.
 *
 * The `pmm_free_pages` function frees `count` physical pages returning ownership of them to the PMM. The PMM will
 * determine based on the addresses if they're owned by the bitmap or the free stack.
 *
 * @param addresses An array containing the higher half physical addresses of the pages to free.
 * @param count The number of pages to free.
 */
void pmm_free_pages(void** addresses, uint64_t count);

/**
 * @brief Frees a contiguous region of physical pages.
 *
 * The `pmm_free_region` function frees a contiguous block of `count` physical pages, returning ownership of them to the
 * PMM. The PMM will determine based on the address if it's owned by the bitmap or the free stack.
 *
 * @param address The higher half physical address of the first page in the region to free.
 * @param count The number of pages to free.
 */
void pmm_free_region(void* address, uint64_t count);

/**
 * @brief Retrieves the total amount of physical memory managed by the PMM.
 *
 * @return The total amount of physical memory in pages.
 */
uint64_t pmm_total_amount(void);

/**
 * @brief Retrieves the amount of free physical memory.
 *
 * @return The amount of currently free physical memory in pages.
 */
uint64_t pmm_free_amount(void);

/**
 * @brief Retrieves the amount of reserved physical memory.
 *
 * Reserved memory includes memory that is not available for allocation (e.g., kernel code, hardware regions).
 *
 * @return The amount of reserved physical memory in pages.
 */
uint64_t pmm_used_amount(void);

/** @} */
