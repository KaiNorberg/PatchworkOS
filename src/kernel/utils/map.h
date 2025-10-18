#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/io.h>

/**
 * @brief Hash Map
 * @defgroup kernel_utils_map Hash Map
 * @ingroup kernel_utils
 *
 * @{
 */

/**
 * @brief The initial capacity of a map.
 */
#define MAP_INITIAL_CAPACITY 16

/**
 * @brief The maximum load percentage of a map before it resizes.
 */
#define MAP_MAX_LOAD_PERCENTAGE 75

/**
 * @brief The value used to indicate a tombstone (deleted entry).
 */
#define MAP_TOMBSTONE ((map_entry_t*)1)

/**
 * @brief The maximum length of a key in the map.
 */
#define MAP_KEY_MAX_LENGTH 40

/**
 * @brief Map key stucture.
 *
 * Is used to implement a generic key for the map. The object is copied into `key` and hashed.
 * We can then use the hash for quick comparisons and the key irself for full comparisons no matter
 * the type of the key.
 * The maximum size of the key is `MAX_PATH` bytes.
 */
typedef struct
{
    uint8_t key[MAP_KEY_MAX_LENGTH];
    uint64_t len;
    uint64_t hash;
} map_key_t;

/**
 * @brief Map entry structure.
 *
 * Place this in a structure to make it adable to a map and then use `CONTAINER_OF` to get the structure back.
 */
typedef struct
{
    map_key_t key;
} map_entry_t;

/**
 * @brief Hash map structure.
 */
typedef struct
{
    map_entry_t** entries;
    uint64_t capacity;
    uint64_t length;
    uint64_t tombstones;
} map_t;

/**
 * @brief Map iterator structure.
 */
typedef struct
{
    map_t* map;
    uint64_t index;
    map_entry_t* current;
} map_iter_t;

/**
 * @brief Hash a object.
 *
 * @param object The object to hash.
 * @param length The length of the object in bytes.
 * @return The hash of the object.
 */
uint64_t hash_object(const void* object, uint64_t length);

/**
 * @brief Create a map key from a buffer.
 *
 * @param buffer The buffer to create the key from.
 * @param length The length of the buffer in bytes.
 * @return The map key.
 */
static inline map_key_t map_key_buffer(const void* buffer, uint64_t length)
{
    assert(length <= MAP_KEY_MAX_LENGTH);
    map_key_t key;
    memcpy(key.key, buffer, length);
    key.len = length;
    key.hash = hash_object(buffer, length);
    return key;
}

/**
 * @brief Create a map key from a uint64_t.
 *
 * @param uint64 The uint64_t to create the key from.
 * @return The map key.
 */
static inline map_key_t map_key_uint64(uint64_t uint64)
{
    map_key_t key;
    memcpy(key.key, &uint64, sizeof(uint64_t));
    key.len = sizeof(uint64_t);
    key.hash = hash_object(&uint64, sizeof(uint64_t));
    return key;
}

/**
 * @brief Create a map key from a string.
 *
 * @param str The string to create the key from.
 * @return The map key.
 */
static inline map_key_t map_key_string(const char* str)
{
    return map_key_buffer(str, strlen(str));
}

/**
 * @brief Create a map key from a generic object.
 *
 * @param generic The generic object to create the key from.
 * @return The map key.
 */
#define MAP_KEY_GENERIC(generic) map_key_buffer(generic, sizeof(typeof(*generic)))

/**
 * @brief Initialize a map entry.
 *
 * @param entry The map entry to initialize.
 */
void map_entry_init(map_entry_t* entry);

/**
 * @brief Initialize a map.
 *
 * @param map The map to initialize.
 * @return On success, 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t map_init(map_t* map);

/**
 * @brief Deinitialize a map.
 *
 * @param map The map to deinitialize.
 */
void map_deinit(map_t* map);

/**
 * @brief Insert a key-value pair into the map.
 *
 * If the key already exists, then an `EEXIST` error is returned.
 *
 * @param map The map to insert into.
 * @param key The key to insert.
 * @param value The value to insert.
 * @return On success, 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t map_insert(map_t* map, const map_key_t* key, map_entry_t* value);

/**
 * @brief Get a value from the map by key.
 *
 * @param map The map to get from.
 * @param key The key to get.
 * @return On success, the value. If the key does not exist, returns `NULL` and sets `errno`.
 */
map_entry_t* map_get(map_t* map, const map_key_t* key);

/**
 * @brief Remove a key-value pair from the map.
 *
 * If the key does not exist, nothing happens.
 *
 * @param map The map to remove from.
 * @param key The key to remove.
 */
void map_remove(map_t* map, const map_key_t* key);

/**
 * @brief Get the number of entries in the map.
 *
 * @param map The map to get the size of.
 * @return The number of entries in the map.
 */
uint64_t map_size(const map_t* map);

/**
 * @brief Get the capacity of the map.
 *
 * @param map The map to get the capacity of.
 * @return The capacity of the map.
 */
uint64_t map_capacity(const map_t* map);

/**
 * @brief Check if the map is empty.
 *
 * @param map The map to check.
 * @return `true` if the map is empty, `false` otherwise.
 */
bool map_is_empty(const map_t* map);

/**
 * @brief Check if the map contains a key.
 *
 * @param map The map to check.
 * @param key The key to check for.
 * @return `true` if the map contains the key, `false` otherwise.
 */
bool map_contains(map_t* map, const map_key_t* key);

/**
 * @brief Clear all entries from the map.
 *
 * Note that this does not free the entries themselves, only removes them from the map.
 *
 * @param map The map to clear.
 */
void map_clear(map_t* map);

/**
 * @brief Reserve space in the map for at least `minCapacity` entries.
 *
 * @param map The map to reserve space in.
 * @param minCapacity The minimum capacity to reserve.
 * @return On success, 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t map_reserve(map_t* map, uint64_t minCapacity);

/**
 * @brief Initialize a map iterator.
 *
 * @param iter The iterator to initialize.
 * @param map The map to iterate over.
 */
void map_iter_init(map_iter_t* iter, map_t* map);

/**
 * @brief Get the next entry in the map.
 *
 * @param iter The iterator.
 * @return The next entry in the map, or `NULL` if there are no more entries.
 */
map_entry_t* map_iter_next(map_iter_t* iter);

/**
 * @brief Check if the iterator has more entries.
 *
 * @param iter The iterator.
 * @return `true` if the iterator has more entries, `false` otherwise.
 */
bool map_iter_has_next(const map_iter_t* iter);

/**
 * @brief Helper macro to iterate over all entries in a map.
 *
 * @param elem The loop variable, a pointer to a structure containing the `map_entry_t` member.
 * @param map The map to iterate over.
 * @param member The name of the `map_entry_t` member in the structure.
 */
#define MAP_FOR_EACH(elem, map, member) \
    for (map_iter_t __iter = {0}; \
        map_iter_init(&__iter, map), (elem = CONTAINER_OF(map_iter_next(&__iter), typeof(*elem), member)) != NULL;)

/** @} */
