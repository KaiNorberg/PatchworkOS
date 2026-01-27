#pragma once

#define __STDC_WANT_LIB_EXT1__ 1
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/fs.h>

typedef struct map_entry map_entry_t;
typedef struct map map_t;

/**
 * @brief Hash Map
 * @defgroup libstd_sys_map Hash Map
 * @ingroup libstd
 *
 * A statically allocated hash map implementation, designed such that a seqlock can safely be used to protect it.
 *
 * @{
 */

/**
 * @brief Hash a generic buffer.
 *
 * @param buffer The buffer to hash.
 * @param length The length of the buffer in bytes.
 * @return The hash of the buffer.
 */
static inline uint64_t hash_buffer(const void* buffer, size_t length)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;

    for (uint64_t i = 0; i < length; ++i)
    {
        hash ^= (uint64_t)((uint8_t*)buffer)[i];
        hash *= prime;
    }

    return hash;
}

/**
 * @brief Hash a 64-bit integer.
 *
 * @param x The integer to hash.
 * @return The hash.
 */
static inline uint64_t hash_uint64(uint64_t x)
{
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}

/**
 * @brief Hash a null-terminated string.
 * 
 * @param str The string to hash.
 * @return The hash.
 */
static inline uint64_t hash_string(const char* str)
{
    return hash_buffer(str, strlen(str));
}

/**
 * @brief Intrusive map entry structure.
 * @struct map_entry_t
 */
typedef struct map_entry
{
    map_entry_t* next;
} map_entry_t;

/**
 * @brief Initialize a map entry.
 *
 * @param entry The entry to initialize.
 */
static inline void map_entry_init(map_entry_t* entry)
{
    entry->next = NULL;
}

/**
 * @brief Map comparison function type.
 *
 * @param entry The entry to compare.
 * @param key The key to compare to.
 * @param `true` if the entry and key are equal, `false` otherwise.
 */
typedef bool (*map_cmp_t)(map_entry_t* entry, const void* key);

/**
 * @brief Map structure.
 * @struct map_t
 */
typedef struct map
{
    map_entry_t** buckets;
    size_t size;
    map_cmp_t cmp;
} map_t;

/**
 * @brief Define a map and its buffer.
 * 
 * @param _name The name of the map variable.
 * @param _size The size of the buckets buffer for the map.
 * @param _cmp The comparision function to use.
 */
#define MAP_CREATE(_name, _size, _cmp) \
    map_entry_t* _name##_buckets[_size] = {0}; \
    map_t _name = {.size = (_size), .cmp = (_cmp), .buckets = _name##_buckets};

/**
 * @brief Define a map and its buckets buffer.
 *
 * Intended to be used for struct members.
 *
 * @param _name The name of the map member.
 * @param _size The number of buckets.
 */
#define MAP_DEFINE(_name, _size) \
    map_entry_t* _name##Buckets[_size]; \
    map_t _name

/**
 * @brief Initialize a map defined with `MAP_DEFINE`.
 *
 * @param _map The map member.
 * @param _cmp The comparison function.
 */
#define MAP_DEFINE_INIT(_map, _cmp) \
    map_init(&(_map), (_map##Buckets), ARRAY_SIZE(_map##Buckets), (_cmp)); \
    memset((_map##Buckets), 0, sizeof((_map##Buckets)))

/**
 * @brief Initialize a buffer structure.
 * 
 * @param map The map to initialize.
 * @param buckets The buffer to store the map buckets.
 * @param size The size of the buckets buffer.
 * @param cmp The comparison function for the map.
 */
static inline void map_init(map_t* map, map_entry_t** buckets, size_t size, map_cmp_t cmp)
{
    map->size = size;
    map->buckets = buckets;
    map->cmp = cmp;
}

/**
 * @brief Finds an entry in the map.
 * 
 * @param map The map to search.
 * @param key The key of the entry to find.
 * @param hash The hashed key.
 * @return On success, the entry. On failure, `NULL`.
 */
static inline map_entry_t* map_find(map_t* map, const void* key, uint64_t hash)
{
    map_entry_t* entry = map->buckets[hash % map->size];
    for (; entry != NULL; entry = entry->next)
    {
        if (map->cmp(entry, key))
        {
            return entry;
        }
    }

    return NULL;
}

/**
 * @brief Insert an entry into the map-
 * 
 * @param map The map to insert into.
 * @param entry The entry to add.
 * @param hash The hash of the entry.
 */
static inline void map_insert(map_t* map, map_entry_t* entry, uint64_t hash)
{
    entry->next = map->buckets[hash % map->size];
    map->buckets[hash % map->size] = entry;
}

/**
 * @brief Remove an entry from the map.
 *
 * @param map The map to remove form.
 * @param entry The entry to remove.
 * @param hash The hash of the entry.
 */
static inline void map_remove(map_t* map, map_entry_t* entry, uint64_t hash)
{
    map_entry_t** prev = &map->buckets[hash % map->size];
    for (; *prev != NULL; prev = &(*prev)->next)
    {
        if (*prev == entry)
        {
            *prev = entry->next;
            break;
        }
    }
    entry->next = NULL;
}

/**
 * @brief Finds an entry in the map and removes it.
 * 
 * @param map The map to search.
 * @param key The key of the entry to find.
 * @param hash The hashed key.
 * @return On success, the entry. On failure, `NULL`.
 */
static inline map_entry_t* map_find_and_remove(map_t* map, const void* key, uint64_t hash)
{
    map_entry_t* entry = map_find(map, key, hash);
    if (entry != NULL)
    {
        map_remove(map, entry, hash);
    }
    return entry;
}

/**
 * @brief Helper macro for iterating over all elements in a map.
 *
 * @param _elem The loop variable, a pointer to the structure containing the map entry.
 * @param _map A pointer to the `map_t` structure to iterate over.
 * @param _member The name of the `map_entry_t` member within the structure `elem`.
 */
#define MAP_FOR_EACH(_elem, _map, _member) \
    for (size_t _i = 0; _i < (_map)->size; ++_i) \
        for (map_entry_t* _entry = (_map)->buckets[_i]; \
             _entry != NULL && ((_elem) = CONTAINER_OF(_entry, typeof(*(_elem)), _member), true); \
             _entry = _entry->next)

/**
 * @brief Safely iterates over a map, allowing for element removal during iteration.
 *
 * @param _elem The loop variable, a pointer to the structure containing the map entry.
 * @param _temp A temporary loop variable, a pointer to the structure containing the next map entry.
 * @param _map A pointer to the `map_t` structure to iterate over.
 * @param _member The name of the `map_entry_t` member within the structure `elem`.
 */
#define MAP_FOR_EACH_SAFE(_elem, _temp, _map, _member) \
    for (size_t _i = 0; _i < (_map)->size; ++_i) \
        for (map_entry_t *_entry = (_map)->buckets[_i], *_next = _entry ? _entry->next : NULL; \
             _entry != NULL && \
             ((_elem) = CONTAINER_OF(_entry, typeof(*(_elem)), _member), true) && \
             ((_temp) = _next ? CONTAINER_OF(_next, typeof(*(_elem)), _member) : NULL, true); \
             _entry = _next, _next = _entry ? _entry->next : NULL)

/** @} */
