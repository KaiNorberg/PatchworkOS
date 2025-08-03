#pragma once

#include <sys/proc.h>

/**
 * @brief A generic free stack page allocator.
 * @defgroup kernel_mem_pmm_stack PMM Stack
 * @ingroup kernel_mem
 *
 * The PMM stack provides a fast, O(1) allocator for single pages. It uses freed pages to store metadata about other
 * free pages, forming a stack of page buffers.
 *
 */

/**
 * @brief Structure for a page buffer in the PMM stack.
 * @ingroup kernel_mem_pmm_stack
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
 * @ingroup kernel_mem_pmm_stack
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
    /**
     * @brief The number of free pages in the stack.
     */
    uint64_t free;
} pmm_stack_t;

/**
 * @brief Initializes a PMM stack.
 * @ingroup kernel_mem_pmm_stack
 *
 * @param stack The stack to initialize.
 */
void pmm_stack_init(pmm_stack_t* stack);

/**
 * @brief Allocates a single page from the stack.
 * @ingroup kernel_mem_pmm_stack
 *
 * @param stack The stack to allocate from.
 * @return On success, a pointer to the allocated page. On failure `NULL` and errno is set.
 */
void* pmm_stack_alloc(pmm_stack_t* stack);

/**
 * @brief Frees a single page, returning it to the stack.
 * @ingroup kernel_mem_pmm_stack
 *
 * @param stack The stack to free to.
 * @param address The address of the page to free.
 */
void pmm_stack_free(pmm_stack_t* stack, void* address);
