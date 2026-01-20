#pragma once

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
    void* page;      ///< Pointer to the first page of the segment in the higher half.
    uint32_t length; ///< Length of the segment in bytes.
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
 * @return On success, a pointer to the allocated Memory Descriptor. On failure, `NULL` and `errno` is set.
 */
static inline mem_desc_t* mem_desc_new(mem_desc_pool_t* pool)
{
    pool_idx_t idx = pool_alloc(&pool->pool);
    if (idx == POOL_IDX_MAX)
    {
        errno = ENOSPC;
        return NULL;
    }

    mem_desc_t* desc = &pool->descs[idx];
    desc->next = POOL_IDX_MAX;
    desc->amount = 0;
    desc->capacity = 0;
    desc->size = 0;
    desc->large = NULL;
    return desc;
}

/**
 * @brief Free a Memory Descriptor back to its pool.
 *
 * @param desc Pointer to the Memory Descriptor to free.
 */
static inline void mem_desc_free(mem_desc_t* desc)
{
    mem_desc_pool_t* pool = mem_desc_pool_get(desc);
    pool_free(&pool->pool, desc->next);
}

static inline size_t mem_desc_size(mem_desc_t* desc)
{
    return desc->size;
}

uint64_t mem_desc_add(mem_desc_t* desc, void* addr, size_t size);

static inline uint64_t mem_desc_populate_user(mem_desc_t* desc, process_t* process, void* addr, size_t size)
{
}

/** @} */