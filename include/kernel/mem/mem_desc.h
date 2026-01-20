#pragma once

#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/pool.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/list.h>

typedef struct process process_t;

/**
 * @brief Memory Descriptor.
 * @defgroup kernel_mem_mem_desc Memory Descriptor
 * @ingroup kernel_mem
 *
 * @{
 */

/**
 * @brief Amount of memory segments statically allocated for small descriptors.
 */
#define MEM_SEGS_SMALL_MAX 2

/**
 * @brief Memory Segment structure.
 * @struct mem_seg_t
 */
typedef struct mem_seg
{
    pfn_t pfn;       ///< Page frame number.
    uint32_t size;   ///< Size of the segment in bytes.
    uint32_t offset; ///< Offset in bytes within the first page.
} mem_seg_t;

/**
 * @brief Memory Descriptor structure.
 * @struct mem_desc_t
 */
typedef struct ALIGNED(64) mem_desc
{
    pool_idx_t next;                     ///< Index of the next descriptor or used for the pools free list.
    pool_idx_t index;                    ///< Index of this descriptor in its pool.
    uint32_t amount;                     ///< Number of memory segments.
    uint32_t capacity;                   ///< Capacity of the `large` array.
    size_t size;                         ///< Total size of the memory region in bytes.
    mem_seg_t small[MEM_SEGS_SMALL_MAX]; ///< Statically allocated segments for small regions.
    mem_seg_t* large;                    ///< Pointer to additional segments for large regions.
} mem_desc_t;

/**
 * @brief Memory Descriptor Pool structure.
 * @struct mem_desc_pool_t
 */
typedef struct
{
    pool_t pool;
    mem_desc_t descs[];
} mem_desc_pool_t;

/**
 * @brief Allocate a new Memory Descriptor pool.
 *
 * @param size The amount of descriptors to allocate.
 * @return On success, a pointer to the new Memory Descriptor pool. On failure, `NULL` and `errno` is set.
 */
mem_desc_pool_t* mem_desc_pool_new(size_t size);

/**
 * @brief Free a Memory Descriptor pool.
 *
 * @param pool Pointer to the Memory Descriptor pool to free.
 */
void mem_desc_pool_free(mem_desc_pool_t* pool);

/**
 * @brief Retrieve the Memory Descriptor pool that a Memory Descriptor was allocated from.
 *
 * @param desc Pointer to the Memory Descriptor.
 * @return Pointer to the Memory Descriptor pool.
 */
static inline mem_desc_pool_t* mem_desc_pool_get(mem_desc_t* desc)
{
    return CONTAINER_OF(desc, mem_desc_pool_t, descs[desc->index]);
}

/**
 * @brief Allocate a new Memory Descriptor from a pool.
 *
 * @param pool Pointer to the Memory Descriptor pool.
 * @return On success, a pointer to the allocated Memory Descriptor. On failure, `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOSPC`: No space available in the pool.
 */
mem_desc_t* mem_desc_new(mem_desc_pool_t* pool);

/**
 * @brief Free a Memory Descriptor back to its pool.
 *
 * @param desc Pointer to the Memory Descriptor to free.
 */
void mem_desc_free(mem_desc_t* desc);

/**
 * @brief Add a physical memory region to the Memory Descriptor.
 *
 * @param desc Pointer to the Memory Descriptor.
 * @param phys The physical address of the memory region.
 * @param size The size of the memory region in bytes.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EFAULT`: The physical address is not allocated.
 * - `ENOMEM`: Not enough memory.
 */
uint64_t mem_desc_add(mem_desc_t* desc, phys_addr_t phys, size_t size);

/**
 * @brief Add a user space memory region to the Memory Descriptor.
 *
 * @param desc Pointer to the Memory Descriptor.
 * @param process The process the user space memory region belongs to.
 * @param addr The virtual address of the user space memory region.
 * @param size The size of the user space memory region in bytes.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - See `mem_desc_add()` for possible error codes.
 */
uint64_t mem_desc_add_user(mem_desc_t* desc, process_t* process, const void* addr, size_t size);

/**
 * @brief Read from a Memory Descriptor into a buffer.
 * 
 * @param desc The Memory Descriptor to read from.
 * @param buffer The buffer to read into.
 * @param count Number of bytes to read.
 * @param offset Offset within the Memory Descriptor to start reading from.
 * @return The number of bytes read.
 */
uint64_t mem_desc_read(mem_desc_t* desc, void* buffer, size_t count, size_t offset);

/**
 * @brief Write to a Memory Descriptor from a buffer.
 * 
 * @param desc The Memory Descriptor to write to.
 * @param buffer The buffer to write from.
 * @param count Number of bytes to write.
 * @param offset Offset within the Memory Descriptor to start writing to.
 * @return The number of bytes written. 
 */
uint64_t mem_desc_write(mem_desc_t* desc, const void* buffer, size_t count, size_t offset);

/**
 * @brief Copy data between two Memory Descriptors.
 * 
 * @param dest The destination Memory Descriptor.
 * @param destOffset Offset within the destination Memory Descriptor to start writing to.
 * @param src The source Memory Descriptor.
 * @param srcOffset Offset within the source Memory Descriptor to start reading from.
 * @param count Number of bytes to copy.
 * @return The number of bytes copied.
 */
uint64_t mem_desc_copy(mem_desc_t* dest, size_t destOffset, mem_desc_t* src, size_t srcOffset, size_t count);

/**
 * @brief Iterate over objects within a Memory Descriptor.
 *
 * @param _element The iterator variable.
 * @param _desc Pointer to the Memory Descriptor.
 */
#define MEM_DESC_FOR_EACH(_element, _desc)

/** @} */