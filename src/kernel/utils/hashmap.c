#include "hashmap.h"

#include "mem/heap.h"

#include <stdlib.h>

static uint64_t hash_uint64(uint64_t key)
{
    key = (key ^ (key >> 33)) * 0xff51afd7ed558ccd;
    key = (key ^ (key >> 33)) * 0xc4ceb9fe1a85ec53;
    key = key ^ (key >> 33);
    return key;
}

void hashmap_entry_init(hashmap_entry_t* entry)
{
    entry->key = ERR;
}

static const uint64_t hashmap_insert_no_grow(hashmap_t* map, uint64_t key, hashmap_entry_t* entry)
{
    // AND hash with capacity-1 to ensure it's within entries array.
    uint64_t hash = hash_uint64(key);
    size_t index = (size_t)(hash & (uint64_t)(map->capacity - 1));

    while (map->entries[index] != NULL)
    {
        if (map->entries[index]->key == key)
        {
            return ERR;
        }

        index++;

        if (index >= map->capacity)
        {
            index = 0;
        }
    }

    map->length++;
    map->entries[index] = entry;
    entry->key = key;
    return key;
}

static uint64_t hashmap_grow(hashmap_t* map)
{
    uint64_t newCapacity = map->capacity * 2;
    if (newCapacity < map->capacity) // Overflow
    {
        return ERR;
    }

    hashmap_entry_t** newEntries = heap_calloc(newCapacity, sizeof(hashmap_entry_t*), HEAP_NONE);
    if (newEntries == NULL)
    {
        return false;
    }

    uint64_t oldCapacity = map->capacity;
    hashmap_entry_t** oldEntries = map->entries;

    map->capacity = newCapacity;
    map->entries = newEntries;
    map->length = 0;

    for (size_t i = 0; i < map->capacity; i++)
    {
        hashmap_entry_t* entry = map->entries[i];
        if (entry != NULL)
        {
            if (hashmap_insert_no_grow(map, entry->key, entry) == ERR)
            {
                heap_free(newEntries);
                map->capacity = oldCapacity;
                map->entries = oldEntries;
                map->length = 0;

                return ERR;
            }
        }
    }

    heap_free(oldEntries);
    return true;
}

uint64_t hashmap_init(hashmap_t* map)
{
    map->length = 0;
    map->capacity = HASHMAP_INITIAL_CAPACITY;

    map->entries = heap_calloc(map->capacity, sizeof(hashmap_entry_t*), HEAP_NONE);
    if (map->entries == NULL)
    {
        return ERR;
    }

    return 0;
}

void hashmap_deinit(hashmap_t* map)
{
    heap_free(map->entries);
    map->entries = NULL;
    map->length = 0;
    map->capacity = 0;
}

uint64_t hashmap_insert(hashmap_t* map, uint64_t key, hashmap_entry_t* entry)
{
    if (entry == NULL)
    {
        return ERR;
    }

    if (map->length >= map->capacity / 2)
    {
        if (!hashmap_grow(map))
        {
            return ERR;
        }
    }

    return hashmap_insert_no_grow(map, key, entry);
}

hashmap_entry_t* hashmap_get(hashmap_t* map, uint64_t key)
{
    uint64_t hash = hash_uint64(key);
    size_t index = (size_t)(hash & (uint64_t)(map->capacity - 1)); // Capacity is a power of 2

    while (map->entries[index] != NULL)
    {
        if (map->entries[index]->key == key)
        {
            return map->entries[index];
        }

        index++;

        if (index >= map->capacity)
        {
            index = 0;
        }
    }

    return NULL;
}
