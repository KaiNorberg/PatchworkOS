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
 * TODO: Dynamically sized keys without heap allocation?
 *
 * @{
 */

/**
 * @brief The minimum capacity of a map.
 */
#define MAP_MIN_CAPACITY 16

/**
 * @brief The maximum load percentage of a map before it resizes.
 */
#define MAP_MAX_LOAD_PERCENTAGE 75

/**
 * @brief The value used to indicate a tombstone (removed entry).
 */
#define MAP_TOMBSTONE ((map_entry_t*)1)

/**
 * @brief The maximum length of a key in the map.
 */
#define MAP_KEY_MAX_LENGTH 118

/**
 * @brief Map key stucture.
 *
 * Is used to implement a generic key for the map. The object is copied into `key` and hashed.
 * We can then use the hash for quick comparisons and lookups while using the key itself for full comparisons no matter
 * the type of the key.
 */
typedef struct
{
    uint8_t key[MAP_KEY_MAX_LENGTH];
    uint8_t len;
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
 * @brief Check if a map entry pointer is valid (not NULL or tombstone).
 *
 * @param entryPtr The map entry pointer to check.
 * @return true if the entry pointer is valid, false otherwise.
 */
#define MAP_ENTRY_PTR_IS_VALID(entryPtr) ((entryPtr) != NULL && (entryPtr) != MAP_TOMBSTONE)

/**
 * @brief Hash map structure.
 *
 * The entries can be safely iterated over as an array as long sa the `MAP_ENTRY_PTR_IS_VALID` macro is used to check
 * each entry before dereferencing it.
 */
typedef struct
{
    map_entry_t** entries;
    uint64_t capacity;
    uint64_t length;
    uint64_t tombstones;
} map_t;

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
    assert(length < MAP_KEY_MAX_LENGTH);
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
    return map_key_buffer(str, strnlen_s(str, MAP_KEY_MAX_LENGTH));
}

/**
 * @brief Initialize a map entry.
 *
 * @param entry The map entry to initialize.
 */
void map_entry_init(map_entry_t* entry);

/**
 * @brief Create a map initializer.
 *
 * @return A map initializer.
 */
#define MAP_CREATE {.entries = NULL, .capacity = 0, .length = 0, .tombstones = 0}

/**
 * @brief Initialize a map.
 *
 * @param map The map to initialize.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
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
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t map_insert(map_t* map, const map_key_t* key, map_entry_t* value);

/**
 * @brief Get a value from the map by key.
 *
 * @param map The map to get from.
 * @param key The key to get.
 * @return On success, the value. If the key does not exist, returns `NULL`.
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
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t map_reserve(map_t* map, uint64_t minCapacity);

/** @} */
