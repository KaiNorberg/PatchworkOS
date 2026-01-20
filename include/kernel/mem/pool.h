#pragma once

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include <sys/defs.h>

/**
 * @brief Lock-free memory pool.
 * @defgroup kernel_mem_pool Pool
 * @ingroup kernel_mem
 *
 * The memory pool system provides a lock-free allocator using a pre-allocated array. Its intended to be used for
 * performance-critical structures and as a even more specialized alternative to the Object Cache.
 *
 * In addition to its performance advantages, since the pool uses an array to store its objects, it is possible for
 * certain structures to avoid storing full pointers to objects allocated from a pool instead using a `pool_idx_t` and
 * thus saving memory or allowing better caching.
 *
 * @{
 */

/**
 * @brief Pool index type.
 */
typedef uint16_t pool_idx_t;

#define POOL_IDX_MAX UINT16_MAX ///< The maximum index value for pool.

#define POOL_TAG_INC ((uint64_t)(POOL_IDX_MAX) + 1) ///< The amount to increment the tag by in the tagged free list.

/**
 * @brief Pool structure.
 * @struct pool_t
 */
typedef struct
{
    atomic_size_t used;   ///< Number of used elements.
    atomic_uint64_t free; ///< The tagged head of the free list.
    void* elements;       ///< Pointer to the elements array.
    size_t elementSize;   ///< Size of each element.
    size_t nextOffset;    ///< Offset of a `pool_idx_t` variable within each element used for the free list.
    size_t capacity;      ///< Maximum number of elements.
} pool_t;

/**
 * @brief Initialize a pool.
 *
 * @param pool Pointer to the pool structure to initialize.
 * @param elements Pointer to the elements array.
 * @param capacity Maximum number of elements.
 * @param elementSize Size of each element.
 * @param nextOffset Offset of a `pool_idx_t` variable within each element used for the free list.
 */
void pool_init(pool_t* pool, void* elements, size_t capacity, size_t elementSize, size_t nextOffset);

/**
 * @brief Allocate an element from the pool.
 *
 * @param pool Pointer to the pool to allocate from.
 * @return The index of the allocated element, or `POOL_IDX_MAX` if the pool is full.
 */
pool_idx_t pool_alloc(pool_t* pool);

/**
 * @brief Free an element back to the pool.
 *
 * @param pool Pointer to the pool to free to.
 * @param idx The index of the element to free.
 */
void pool_free(pool_t* pool, pool_idx_t idx);

/** @} */