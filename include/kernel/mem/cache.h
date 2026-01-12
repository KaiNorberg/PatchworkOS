#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/cpu/cpu.h>
#include <kernel/sync/lock.h>
#include <sys/list.h>

typedef struct cache cache_t;

/**
 * @brief Object Cache
 * @defgroup kernel_mem_cache Cache
 * @ingroup kernel_mem
 *
 * A object cache using CPU local SLAB allocation to improve performance of frequently allocated and deallocated
 * objects.
 *
 * ## Slab Allocation
 *
 * The object cache uses slab allocation to allocate memory, each slab consists of a buffer in the below format:
 *
 * <div style="text-align: center;">
 * | Size                                  | Description         |
 * | :------------------------------------ | :------------------ |
 * | sizeof(cache_slab_t)                  | Slab metadata       |
 * | (N - 1) * sizeof(cache_bufctl_t)      | Buffer control list |
 * | ...                                   | Padding             |
 * | N * step                              | Objects             |
 * </div>
 *
 * Where N is the number of objects that can fit in the slab given the object size and alignment and the step is the
 * aligned size of the object.
 *
 * ### Buffer Control List
 *
 *
 *
 * @see https://en.wikipedia.org/wiki/Slab_allocation for more information.
 * @see https://www.kernel.org/doc/gorman/html/understand/understand011.html for an explanation of the Linux kernel slab
 * allocator.
 *
 * @{
 */

#define CACHE_LIMIT 16 ///< Maximum number of free slabs in a cache.

typedef uint16_t cache_bufctl_t; ///< Buffer control type.

#define CACHE_BUFCTL_END (0) ///< End of buffer control list marker.

#define CACHE_LINE 64 ///< Cache line size in bytes.

#define CACHE_SLAB_PAGES 64 ///< Number of pages in a slab.

/**
 * @brief Cache slab layout structure.
 * @struct cache_slab_layout_t
 */
typedef struct
{
    uint32_t start;
    uint32_t step; ///< The power of two index for the object size.
    uint32_t amount;
} cache_slab_layout_t;

/**
 * @brief Cache slab structure.
 * @struct cache_slab_t
 */
typedef struct ALIGNED(CACHE_LINE)
{
    list_entry_t entry;
    cpu_id_t owner;
    uint16_t freeCount;
    uint16_t firstFree;
    lock_t lock;
    cache_t* cache;
    void* objects;
    cache_bufctl_t bufctl[] ALIGNED(CACHE_LINE);
} cache_slab_t;

static_assert(sizeof(cache_slab_t) <= 64, "size of cache_slab_t is to large for a single cache line");

/**
 * @brief Per-CPU cache context.
 * @struct cache_cpu_t
 */
typedef struct ALIGNED(CACHE_LINE)
{
    cache_slab_t* active;
} cache_cpu_t;

/**
 * @brief Cache structure.
 * @struct cache_t
 */
typedef struct cache
{
    const char* name;
    size_t size;
    size_t alignment;
    cache_slab_layout_t layout;
    void (*ctor)(void* obj);
    void (*dtor)(void* obj);
    lock_t lock;
    list_t free;
    list_t active;
    list_t full;
    uint64_t freeCount;
    cache_cpu_t cpus[CPU_MAX];
} cache_t;

/**
 * @brief Macro to create a cache initializer.
 *
 * @param _cache The name of the cache variable.
 * @param _name The name of the cache, for debugging purposes.
 * @param _size The size of each object in the cache.
 * @param _alignment The alignment of each object in the cache.
 * @param _ctor The constructor function for objects in the cache.
 * @param _dtor The destructor function for objects in the cache.
 */
#define CACHE_CREATE(_cache, _name, _size, _alignment, _ctor, _dtor) \
    { \
        .name = (_name), \
        .size = (_size), \
        .alignment = (_alignment), \
        .layout = {0}, \
        .ctor = (_ctor), \
        .dtor = (_dtor), \
        .lock = LOCK_CREATE(), \
        .free = LIST_CREATE((_cache).free), \
        .active = LIST_CREATE((_cache).active), \
        .full = LIST_CREATE((_cache).full), \
        .freeCount = 0, \
        .cpus = {0}, \
    }

/**
 * @brief Allocate an object from the cache.
 *
 * The object will be constructed using the cache's constructor if one is provided.
 *
 * @param cache The cache to allocate from.
 * @return Pointer to the allocated object, or `NULL` on failures.
 */
void* cache_alloc(cache_t* cache);

/**
 * @brief Free an object back to its cache.
 *
 * If the cache is full and a destructor is provided then the object will be destructed.
 *
 * @param obj The object to free.
 */
void cache_free(void* obj);

/** @} */