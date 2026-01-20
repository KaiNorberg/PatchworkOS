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
 * @brief Page metadata structure.
 * @struct page_t
 *
 * Used internally by the PMM.
 */
typedef struct
{
    uint16_t ref;
} page_t;

/**
 * @brief Maximum number of free pages that can be stored in a free page.
 */
#define FREE_PAGE_MAX (PAGE_SIZE / sizeof(pfn_t) - 1)

/**
 * @brief Stored in free pages to form a stack of free pages.
 * @struct page_stack_t
 */
typedef struct page_stack
{
    struct page_stack* next;
    pfn_t pages[FREE_PAGE_MAX];
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
 * @return On success, the PFN of the allocated page. On failure, `ERR`.
 */
pfn_t pmm_alloc(void);

/**
 * @brief Allocate multiple pages of physical memory.
 *
 * Usefull for reducing overhead from locking when allocating many pages.
 *
 * @param pfns Array to store the allocated page PFNs.
 * @param count Number of pages to allocate.
 * @return On success, `0`. On failure, `ERR` and no pages are allocated.
 */
uint64_t pmm_alloc_pages(pfn_t* pfns, size_t count);

/**
 * @brief Allocate a contiguous region of physical memory using the bitmap.
 *
 * @param count Number of pages to allocate.
 * @param maxPfn Maximum PFN to allocate up to (exclusive).
 * @param alignPfn Alignment of the region in pages.
 * @return On success, the PFN of the first page of the allocated region. On failure, `ERR`.
 */
pfn_t pmm_alloc_bitmap(size_t count, pfn_t maxPfn, pfn_t alignPfn);

/**
 * @brief Free a single page of physical memory.
 *
 * The page will only be reclaimed if its reference count reaches zero.
 *
 * @param pfn The PFN of the page to free.
 */
void pmm_free(pfn_t pfn);

/**
 * @brief Free multiple pages of physical memory.
 *
 * Useful for reducing overhead from locking when freeing many pages.
 *
 * The pages will only be reclaimed if its reference count reaches zero.
 *
 * @param pfns Array of PFNs to free.
 * @param count Number of pages to free.
 */
void pmm_free_pages(pfn_t* pfns, size_t count);

/**
 * @brief Free a contiguous region of physical memory.
 *
 * The pages will only be reclaimed if its reference count reaches zero.
 *
 * @param pfn The PFN of the first page of the region to free.
 * @param count Number of pages to free.
 */
void pmm_free_region(pfn_t pfn, size_t count);

/**
 * @brief Increment the reference count of a physical region.
 *
 * Will fail if any of the pages are not allocated.
 *
 * @param pfn The PFN of the first physical page.
 * @param count Number of pages to increment the reference count of.
 * @return On success, the new reference count. On failure, `ERR`.
 */
uint64_t pmm_ref_inc(pfn_t pfn, size_t count);

/**
 * @brief Decrement the reference count of a physical region.
 *
 * If the reference count reaches zero, the pages will be freed.
 *
 * @param pfn The PFN of the first physical page.
 * @param count Number of pages to decrement the reference count of.
 */
static inline void pmm_ref_dec(pfn_t pfn, size_t count)
{
    pmm_free_region(pfn, count);
}

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
