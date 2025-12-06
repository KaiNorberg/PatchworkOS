#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Internal Heap Implementation.
 * @defgroup libstd_heap Heap
 * @ingroup libstd
 *
 * We use a "segregated free list" allocator with a set of bins where each bin stores free blocks of size \f$n \cdot 64\f$ bytes where \f$n\f$ is the index of the bin, up to `_HEAP_LARGE_ALLOC_THRESHOLD`. Above this size, blocks are mapped directly.
 *
 * To allow for coalescing of free blocks, all blocks (allocated and free) are additionally stored in a linked list sorted by address. When a block is freed, we check the previous and next blocks in memory to see if they are free, and if so we merge them into a single larger block.
 * 
 * Included is the internal heap allocation, the functions that the kernel and user space should use are the expected `malloc()`, `free()`, `realloc()`, etc functions.
 *
 * @todo Implement a slab allocator.
 *
 * @{
 */

/**
 * The memory alignment in bytes of all allocations on the heap.
 *
 * We use 64 bytes for better cache line alignment on modern CPUs.
 */
#define _HEAP_ALIGNMENT 64

/**
 * Magic number used to validate heap integrity.
 */
#define _HEAP_HEADER_MAGIC 0xDEADBEEF

/**
 * The threshold size for large allocations, above this size allocations will be mapped directly.
 */
#define _HEAP_LARGE_ALLOC_THRESHOLD (PAGE_SIZE * 4)

/**
 * The number of bins for free lists.
 */
#define _HEAP_NUM_BINS (_HEAP_LARGE_ALLOC_THRESHOLD / _HEAP_ALIGNMENT)

/**
 * @brief Flags for heap blocks.
 */
typedef enum
{
    _HEAP_ALLOCATED = 1 << 0, ///< Block is allocated.
    _HEAP_MAPPED = 1 << 1,    ///< Block is not on the heap, but mapped directly, used for large allocations.
    _HEAP_ZEROED = 1 << 2,    ///< Block is zeroed.
} _heap_flags_t;

/**
 * @brief Header for each heap block.
 * @struct _heap_header
 *
 * Contains metadata for each allocated block in the heap.
 *
 * Must have a size that is a multiple of `_HEAP_ALIGNMENT` for cache alignment.
 */
typedef struct _heap_header
{
    uint32_t magic;
    _heap_flags_t flags;
    uint64_t size;
    list_entry_t freeEntry;
    list_entry_t listEntry;
    uint8_t data[];
} _heap_header_t;

static_assert(sizeof(_heap_header_t) % _HEAP_ALIGNMENT == 0, "_heap_header_t size must be multiple of 64");

/**
 * @brief A list of all blocks sorted by address.
 */
extern list_t _heapList;

/**
 * @brief Initialize the heap.
 */
void _heap_init(void);

/**
 * @brief Acquire the heap lock.
 *
 * Must be paired with a call to `_heap_release()`.
 */
void _heap_acquire(void);

/**
 * @brief Release the heap lock.
 */
void _heap_release(void);

/**
 * @brief Get the bin index for a given size.
 *
 * @param size The size to get the bin index for.
 * @return The bin index, or `ERR` if the size is should be treated as a large allocation.
 */
uint64_t _heap_get_bin_index(uint64_t size);

/**
 * @brief Directly maps a new heap block of at least `minSize` bytes.
 *
 * Must be called with the heap acquired.
 *
 * @param minSize The minimum size of the block to create, in bytes.
 * @return On success, pointer to the new heap block header. On failure, `NULL`.
 */
_heap_header_t* _heap_block_new(uint64_t minSize);

/**
 * @brief Splits a heap block into two blocks, the first of `size` bytes and the second with the remaining bytes.
 *
 * Must be called with the heap acquired.
 *
 * @param block The block to split.
 * @param size The size of the first block after the split, in bytes.
 */
void _heap_block_split(_heap_header_t* block, uint64_t size);

/**
 * @brief Adds a block to the appropriate free list.
 *
 * Must be called with the heap acquired.
 *
 * Will not actually free the block, just add it to the free list.
 *
 * Used to keep track of which bins have free blocks using a bitmap.
 *
 * @param block The block to add.
 */
void _heap_add_to_free_list(_heap_header_t* block);

/**
 * @brief Removes a block from its free list.
 *
 * Must be called with the heap acquired.
 *
 * Will not actually allocate the block, just remove it from the free list.
 *
 * Used to keep track of which bins have free blocks using a bitmap.
 *
 * @param block The block to remove.
 */
void _heap_remove_from_free_list(_heap_header_t* block);

/**
 * @brief Allocates a block of memory of given size.
 *
 * Must be called with the heap acquired.
 *
 * @param size The size of memory to allocate, in bytes.
 * @return On success, pointer to the allocated heap block header. On failure, `NULL` and `errno` is set.
 */
_heap_header_t* _heap_alloc(uint64_t size);

/**
 * @brief Frees a previously allocated heap block.
 *
 * Must be called with the heap acquired.
 *
 * Will attempt to coalesce adjacent free blocks.
 *
 * @param block The block to free.
 */
void _heap_free(_heap_header_t* block);

/**
 * @brief Directly maps memory of the given size.
 *
 * In the kernel this function uses the VMM to map new memory, in user space it uses `/dev/zero`.
 *
 * @param size The size of memory to map, in bytes.
 * @return On success, pointer to the mapped memory. On failure, `NULL`.
 */
void* _heap_map_memory(uint64_t size);

/**
 * @brief Unmaps previously mapped memory.
 *
 * In the kernel this function uses the VMM to unmap memory, in user space it uses the `munmap()` function.
 *
 * @param addr The address of the memory to unmap.
 * @param size The size of memory to unmap, in bytes.
 */
void _heap_unmap_memory(void* addr, uint64_t size);

/** @} */
