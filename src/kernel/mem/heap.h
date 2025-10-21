#pragma once

#include <stdint.h>

/**
 * @brief Kernel Heap
 * @defgroup kernel_mem_heap Heap
 * @ingroup kernel_mem
 *
 * @{
 */

#define HEAP_MAX_SLABS 128
#define HEAP_MAX_SLAB_SIZE 0x64000

#define HEAP_ALIGN 64

#define HEAP_LOOKUP_NONE UINT8_MAX

#define HEAP_ALLOC_POISON 0xDEAD
#define HEAP_FREE_POISON 0xBEEF

/**
 * @brief Flags for heap allocations.
 * @enum heap_flags_t
 */
typedef enum
{
    HEAP_NONE = 0,     ///< No flags.
    HEAP_VMM = 1 << 0, ///< Dont use the slab allocator, instead allocate whole pages using the VMM.
} heap_flags_t;

void heap_init(void);

/**
 * @brief Allocates a block of memory from the kernel heap.
 *
 * @param size The size of the block to allocate, must be greater than 0.
 * @param flags Flags to control the allocation.
 * @return On success, a pointer to the allocated block. On failure, `NULL` and `errno` is set.
 */
void* heap_alloc(uint64_t size, heap_flags_t flags);

/**
 * @brief Reallocates a block of memory from the kernel heap.
 *
 * If `oldPtr` is `NULL`, this function behaves like `heap_alloc()`.
 * If `newSize` is 0, this function behaves like `heap_free()`.
 * If the allocation is successful, the contents of the old block are copied to the new block, up to the minimum of
 * the old and new sizes.
 */
void* heap_realloc(void* oldPtr, uint64_t newSize, heap_flags_t flags);

/**
 * @brief Allocates a block of memory from the kernel heap and zeroes it.
 *
 * @param num Number of elements to allocate, must be greater than 0.
 * @param size Size of each element, must be greater than 0.
 * @param flags Flags to control the allocation.
 * @return On success, a pointer to the allocated block. On failure, `NULL` and `errno` is set.
 */
void* heap_calloc(uint64_t num, uint64_t size, heap_flags_t flags);

/**
 * @brief Frees a block of memory allocated from the kernel heap.
 *
 * @param ptr Pointer to the block to free, can be `NULL`.
 */
void heap_free(void* ptr);

/** @} */
