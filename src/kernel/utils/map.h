#pragma once

#include "defs.h"

#include <assert.h>
#include <stdint.h>
#include <sys/io.h>

#define MAP_INITIAL_CAPACITY 16
#define MAP_MAX_LOAD_PERCENTAGE 75
#define MAP_TOMBSTONE ((map_entry_t*)1)

typedef struct
{
    uint8_t key[MAX_PATH];
    uint64_t len;
    uint64_t hash;
} map_key_t;

typedef struct
{
    map_key_t key;
} map_entry_t;

typedef struct
{
    map_entry_t** entries;
    uint64_t capacity;
    uint64_t length;
    uint64_t tombstones;
} map_t;

typedef struct
{
    map_t* map;
    uint64_t index;
    map_entry_t* current;
} map_iter_t;

#define MAP_ENTRY_CREATE() \
    (map_entry_t) \
    { \
        .key = {0} \
    }

uint64_t hash_buffer(const void* buffer, uint64_t length);

static inline map_key_t map_key_buffer(const void* buffer, uint64_t length)
{
    assert(length <= MAX_PATH);
    map_key_t key;
    memcpy(key.key, buffer, length);
    key.len = length;
    key.hash = hash_buffer(buffer, length);
    return key;
}

static inline map_key_t map_key_string(const char* str)
{
    return map_key_buffer(str, strlen(str));
}

#define MAP_KEY_GENERIC(generic) map_key_buffer(generic, sizeof(typeof(*generic)))

void map_entry_init(map_entry_t* entry);

uint64_t map_init(map_t* map);

void map_deinit(map_t* map);

uint64_t map_insert(map_t* map, const map_key_t* key, map_entry_t* value);

map_entry_t* map_get(map_t* map, const map_key_t* key);

uint64_t map_remove(map_t* map, const map_key_t* key);

uint64_t map_size(const map_t* map);

uint64_t map_capacity(const map_t* map);

bool map_is_empty(const map_t* map);

bool map_contains(map_t* map, const map_key_t* key);

void map_clear(map_t* map);

uint64_t map_reserve(map_t* map, uint64_t minCapacity);

void map_iter_init(map_iter_t* iter, map_t* map);

map_entry_t* map_iter_next(map_iter_t* iter);

bool map_iter_has_next(const map_iter_t* iter);

#define MAP_FOR_EACH(map, entry) \
    for (map_iter_t __iter = {0}; map_iter_init(&__iter, map), (entry = map_iter_next(&__iter)) != NULL;)
