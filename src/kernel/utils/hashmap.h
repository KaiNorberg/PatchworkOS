#pragma once

#include "defs.h"

#include <stdint.h>

#define HASHMAP_INITIAL_CAPACITY 16

typedef struct
{
    uint64_t key;
} hashmap_entry_t;

typedef struct
{
    hashmap_entry_t** entries;
    uint64_t capacity;
    uint64_t length;
} hashmap_t;

#define HASHMAP_CONTAINER(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

void hashmap_entry_init(hashmap_entry_t* entry);

uint64_t hashmap_init(hashmap_t* map);

void hashmap_deinit(hashmap_t* map);

uint64_t hashmap_insert(hashmap_t* map, uint64_t key, hashmap_entry_t* value);

hashmap_entry_t* hashmap_get(hashmap_t* map, uint64_t key);
