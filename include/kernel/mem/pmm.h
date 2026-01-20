#pragma once

#include <boot/boot_info.h>

#include <sys/proc.h>

/**
 * @brief Physical Memory Manager (PMM).
 * @defgroup kernel_mem_pmm PMM
 * @ingroup kernel_mem
 *
 * The Physical Memory Manager (PMM) is responsible for allocating and freeing physical memory pages.
 *
 * @note All physical pages are identity mapped to the higher half of the address space, as such, the PMM will always
 * return valid and usable higher half addresses.
 *
 * ## The Free Page Stack
 *
 * For most allocations, the PMM uses a fast `O(1)` stack-based allocator to manage free pages, with the limitation that
 * only single pages can be allocated or freed at a time.
 *
 * ## The Bitmap Allocator
 *
 * For larger, contiguous or aligned allocations, the PMM uses a bitmap allocator. This allocator is slower (`O(n)`) but
 * is usefull for more specialized allocations.
 *
 * ## Reference Counting
 *
 * All allocations from the PMM are referenced counted, meaning that a page is only freed when its reference count
 * reaches zero. This allows pages to be passed around between subsystems without fear of double frees or
 * use-after-frees.
 *
 * @see kernel_mem_mem_desc
 *
 * @{
 */

/**
 * @brief The type used for page reference counts.
 */
typedef uint16_t pmm_ref_t;

/**
 * @brief Invalid page reference count.
 */
#define PAGE_REF_MAX UINT16_MAX

/**
 * @brief Maximum number of free pages that can be stored in a free page.
 */
#define FREE_PAGE_MAX (PAGE_SIZE / sizeof(void*) - 1)

/**
 * @brief Stored in free pages to form a stack of free pages.
 * @struct page_stack_t
 */
typedef struct page_stack
{
    struct page_stack* next;
    void* pages[FREE_PAGE_MAX];
} page_stack_t;

static_assert(sizeof(page_stack_t) == PAGE_SIZE, "page_stack_t must be exactly one page in size");

/**
 * @brief Read the boot info memory map and initialize the PMM.
 */
void pmm_init(void);

/**
 * @brief Allocate a single page of physical memory.
 *
 * Will by default use the free stack allocator, but if no pages are available there, it will fall back to the bitmap
 * allocator.
 *
 * @return Pointer to the allocated page, or `NULL` if no memory is available.
 */
void* pmm_alloc(void);

/**
 * @brief Allocate multiple pages of physical memory.
 *
 * Usefull for reducing overhead from locking when allocating many pages.
 *
 * @param addresses Array to store the allocated page addresses.
 * @param count Number of pages to allocate.
 * @return On success, `0`. On failure, `ERR` and no pages are allocated.
 */
uint64_t pmm_alloc_pages(void** addresses, size_t count);

/**
 * @brief Allocate a contiguous region of physical memory using the bitmap.
 *
 * @param count Number of pages to allocate.
 * @param maxAddr Maximum address to allocate up to (exclusive).
 * @param alignment Alignment of the region.
 * @return Pointer to the first allocated page, or `NULL` if no memory is available.
 */
void* pmm_alloc_bitmap(size_t count, uintptr_t maxAddr, uint64_t alignment);

/**
 * @brief Free a single page of physical memory.
 *
 * The page will only be reclaimed if its reference count reaches zero.
 *
 * @param address Pointer to the page to free.
 */
void pmm_free(void* address);

/**
 * @brief Free multiple pages of physical memory.
 *
 * Useful for reducing overhead from locking when freeing many pages.
 *
 * The pages will only be reclaimed if its reference count reaches zero.
 *
 * @param addresses Array of pointers to the pages to free.
 * @param count Number of pages to free.
 */
void pmm_free_pages(void** addresses, size_t count);

/**
 * @brief Free a contiguous region of physical memory.
 *
 * The pages will only be reclaimed if its reference count reaches zero.
 *
 * @param address Pointer to the first page of the region to free.
 * @param count Number of pages to free.
 */
void pmm_free_region(void* address, size_t count);

/**
 * @brief Increment the reference count of a physical page.
 *
 * Will fail if the page is not allocated.
 *
 * @param address Address of the physical page.
 * @return On success, the new reference count. On failure, `ERR`.
 */
uint64_t pmm_ref_inc(void* address);

/**
 * @brief Get the total number of physical pages.
 *
 * @return Total number of physical pages.
 */
size_t pmm_total_pages(void);

/**
 * @brief Get the number of available physical pages.
 *
 * @return Number of available physical pages.
 */
size_t pmm_avail_pages(void);

/**
 * @brief Get the number of used physical pages.
 *
 * @return Number of used physical pages.
 */
size_t pmm_used_pages(void);

/** @} */
