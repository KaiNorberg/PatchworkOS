#pragma once

#include "defs.h"

#include <stdint.h>

#define MAP_INITIAL_CAPACITY 16
#define MAP_MAX_LOAD_PERCENTAGE 75
#define MAP_TOMBSTONE ((map_entry_t*)1)

typedef enum
{
    MAP_KEY_NONE,
    MAP_KEY_UINT64,
    MAP_KEY_STRING,
} map_key_type_t;

typedef struct
{
    map_key_type_t type;
    union {
        uint64_t uint64;
        const char* str;
        uint64_t raw;
    } data;
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

uint64_t hash_uint64(uint64_t val);
uint64_t hash_string(const char* str);

static inline map_key_t map_key_uint64(uint64_t val)
{
    map_key_t key;
    key.type = MAP_KEY_UINT64;
    key.data.uint64 = val;
    key.hash = hash_uint64(val);
    return key;
}

static inline map_key_t map_key_string(const char* str)
{
    map_key_t key;
    key.type = MAP_KEY_STRING;
    key.data.str = str;
    key.hash = hash_string(str);
    return key;
}

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
