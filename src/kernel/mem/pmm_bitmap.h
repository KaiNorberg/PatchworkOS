#pragma once

#include "utils/bitmap.h"

#include <sys/proc.h>

/**
 * @brief A generic bitmap page allocator.
 * @defgroup kernel_mem_pmm_bitmap PMM Bitmap
 * @ingroup kernel_mem
 *
 * The PMM bitmap provides a flexible allocator for more specific allocations, for example it can handle contiguous
 * pages, specific alignments and allocating below some specified address. This flexibility comes at the cost of
 * performance, so the bitmap should only be used when necessary.
 */

/**
 * @brief Represents a bitmap allocator's state.
 * @ingroup kernel_mem_pmm_bitmap
 */
typedef struct
{
    /**
     * @brief The underlying bitmap used for tracking page status.
     */
    bitmap_t bitmap;
    /**
     * @brief The number of free pages in the bitmap.
     */
    uint64_t free;
    /**
     * @brief The total number of pages managed by the bitmap.
     */
    uint64_t total;
    /**
     * @brief The maximum address managed by the bitmap.
     */
    uintptr_t maxAddr;
} pmm_bitmap_t;

/**
 * @brief Initializes a PMM bitmap.
 * @ingroup kernel_mem_pmm_bitmap
 *
 * @param bitmap The bitmap to initialize.
 * @param buffer The buffer to use for the bitmap data.
 * @param size The number of pages to manage.
 * @param maxAddr The maximum address to manage.
 */
void pmm_bitmap_init(pmm_bitmap_t* bitmap, void* buffer, uint64_t size, uintptr_t maxAddr);

/**
 * @brief Allocates a contiguous region of pages from the bitmap.
 * @ingroup kernel_mem_pmm_bitmap
 *
 * @param bitmap The bitmap to allocate from.
 * @param count The number of pages to allocate.
 * @param maxAddr The maximum address for the allocation.
 * @param alignment The required alignment for the allocation.
 * @return On success, a pointer to the allocated region. On failure `NULL` and errno is set.
 */
void* pmm_bitmap_alloc(pmm_bitmap_t* bitmap, uint64_t count, uintptr_t maxAddr, uint64_t alignment);

/**
 * @brief Frees a region of pages, returning them to the bitmap.
 * @ingroup kernel_mem_pmm_bitmap
 *
 * @param bitmap The bitmap to free to.
 * @param address The address of the region to free.
 * @param count The number of pages to free.
 */
void pmm_bitmap_free(pmm_bitmap_t* bitmap, void* address, uint64_t count);
