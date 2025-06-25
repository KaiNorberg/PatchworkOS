#include "map.h"

#include "mem/heap.h"

#include <stdlib.h>
#include <string.h>

static bool is_power_of_two(uint64_t n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

static uint64_t next_power_of_two(uint64_t n)
{
    if (n <= 1)
    {
        return 2;
    }

    if (is_power_of_two(n))
    {
        return n;
    }

    uint64_t power = 1;
    while (power < n && power < (UINT64_MAX >> 1))
    {
        power <<= 1;
    }
    return power;
}

uint64_t hash_uint64(uint64_t key)
{
    key = (key ^ (key >> 33)) * 0xff51afd7ed558ccd;
    key = (key ^ (key >> 33)) * 0xc4ceb9fe1a85ec53;
    key = key ^ (key >> 33);
    return key;
}

uint64_t hash_string(const char* str)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;

    while (*str)
    {
        hash ^= (uint64_t)*str++;
        hash *= prime;
    }

    return hash;
}

bool map_key_is_equal(const map_key_t* a, const map_key_t* b)
{
    if (a->type != b->type) {
        return false;
    }

    if (a->hash != b->hash) {
        return false;
    }

    switch (a->type) {
        case MAP_KEY_UINT64:
            return a->data.uint64 == b->data.uint64;
        case MAP_KEY_STRING:
            return strcmp(a->data.str, b->data.str) == 0;
        default:
            return false;
    }
}

void map_entry_init(map_entry_t* entry)
{
    entry->key.type = MAP_KEY_NONE;
    entry->key.data.raw = ERR;
    entry->key.hash = 0;
}

static uint64_t map_find_slot(const map_t* map, const map_key_t* key, bool forInsertion)
{
    uint64_t index = (uint64_t)(key->hash & (map->capacity - 1));
    uint64_t firstTombstone = ERR;

    for (uint64_t i = 0; i < map->capacity; i++)
    {
        uint64_t currentIndex = (index + i) & (map->capacity - 1);
        map_entry_t* entry = map->entries[currentIndex];

        if (entry == NULL)
        {
            if (forInsertion && firstTombstone != ERR)
            {
                return firstTombstone;
            }
            return currentIndex;
        }

        if (entry == MAP_TOMBSTONE)
        {
            if (forInsertion && firstTombstone == ERR)
            {
                firstTombstone = currentIndex;
            }
            continue;
        }

        if (map_key_is_equal(&entry->key, key))
        {
            return currentIndex;
        }
    }

    return forInsertion && firstTombstone != ERR ? firstTombstone : ERR;
}

static uint64_t map_resize(map_t* map, uint64_t newCapacity)
{
    if (!is_power_of_two(newCapacity))
    {
        newCapacity = next_power_of_two(newCapacity);
    }

    if (newCapacity <= map->capacity)
    {
        return 0;
    }

    map_entry_t** newEntries = heap_calloc(newCapacity, sizeof(map_entry_t*), HEAP_NONE);
    if (newEntries == NULL)
    {
        return ERR;
    }

    map_entry_t** oldEntries = map->entries;
    uint64_t oldCapacity = map->capacity;
    uint64_t oldLength = map->length;

    map->entries = newEntries;
    map->capacity = newCapacity;
    map->length = 0;
    map->tombstones = 0;

    for (uint64_t i = 0; i < oldCapacity; i++)
    {
        map_entry_t* entry = oldEntries[i];
        if (entry != NULL && entry != MAP_TOMBSTONE)
        {
            uint64_t newIndex = map_find_slot(map, &entry->key, true);
            if (newIndex == ERR)
            {
                heap_free(newEntries);
                map->entries = oldEntries;
                map->capacity = oldCapacity;
                map->length = oldLength;
                return ERR;
            }
            map->entries[newIndex] = entry;
            map->length++;
        }
    }

    heap_free(oldEntries);
    return 0;
}

uint64_t map_init(map_t* map)
{
    map->length = 0;
    map->capacity = MAP_INITIAL_CAPACITY;
    map->tombstones = 0;

    map->entries = heap_calloc(map->capacity, sizeof(map_entry_t*), HEAP_NONE);
    if (map->entries == NULL)
    {
        return ERR;
    }

    return 0;
}

void map_deinit(map_t* map)
{
    heap_free(map->entries);
    map->entries = NULL;
    map->length = 0;
    map->capacity = 0;
    map->tombstones = 0;
}

uint64_t map_insert(map_t* map, const map_key_t* key, map_entry_t* entry)
{
    if (entry == NULL)
    {
        return ERR;
    }

    uint32_t currentEntries = map->length + map->tombstones;
    if ((currentEntries * 100) / map->capacity >= MAP_MAX_LOAD_PERCENTAGE)
    {
        if (map_resize(map, map->capacity * 2) != 0)
        {
            return ERR;
        }
    }

    uint64_t index = map_find_slot(map, key, true);
    if (index == ERR)
    {
        return ERR;
    }

    if (map->entries[index] != NULL && map->entries[index] != MAP_TOMBSTONE && map_key_is_equal(&map->entries[index]->key, key))
    {
        return ERR;
    }

    if (map->entries[index] == MAP_TOMBSTONE)
    {
        map->tombstones--;
    }

    map->entries[index] = entry;
    entry->key = *key;
    map->length++;

    return 0;
}

map_entry_t* map_get(map_t* map, const map_key_t* key)
{
    uint64_t index = map_find_slot(map, key, false);
    if (index == ERR)
    {
        return NULL;
    }

    map_entry_t* entry = map->entries[index];
    if (entry == NULL || entry == MAP_TOMBSTONE)
    {
        return NULL;
    }

    if (map_key_is_equal(&entry->key, key)) {
        return entry;
    }

    return NULL;
}

uint64_t map_remove(map_t* map, const map_key_t* key)
{
    uint64_t index = map_find_slot(map, key, false);
    if (index == ERR)
    {
        return ERR;
    }

    map_entry_t* entry = map->entries[index];
    if (entry == NULL || entry == MAP_TOMBSTONE || !map_key_is_equal(&entry->key, key))
    {
        return ERR;
    }

    map->entries[index] = MAP_TOMBSTONE;
    map->length--;
    map->tombstones++;

    return 0;
}

uint64_t map_size(const map_t* map)
{
    return map->length;
}

uint64_t map_capacity(const map_t* map)
{
    return map->capacity;
}

bool map_is_empty(const map_t* map)
{
    return map->length == 0;
}

bool map_contains(map_t* map, const map_key_t* key)
{
    return map_get(map, key) != NULL;
}

void map_clear(map_t* map)
{
    for (uint64_t i = 0; i < map->capacity; i++)
    {
        map->entries[i] = NULL;
    }

    map->length = 0;
    map->tombstones = 0;
}

uint64_t map_reserve(map_t* map, uint64_t minCapacity)
{
    if (minCapacity <= map->capacity)
    {
        return 0;
    }

    uint64_t newCapacity = next_power_of_two(minCapacity);
    return map_resize(map, newCapacity);
}

void map_iter_init(map_iter_t* iter, map_t* map)
{
    iter->map = map;
    iter->index = 0;
    iter->current = NULL;
}

map_entry_t* map_iter_next(map_iter_t* iter)
{
    map_t* map = iter->map;

    while (iter->index < map->capacity)
    {
        map_entry_t* entry = map->entries[iter->index];
        iter->index++;

        if (entry != NULL && entry != MAP_TOMBSTONE)
        {
            iter->current = entry;
            return entry;
        }
    }

    iter->current = NULL;
    return NULL;
}

bool map_iter_has_next(const map_iter_t* iter)
{
    map_t* map = iter->map;

    for (uint64_t i = iter->index; i < map->capacity; i++)
    {
        map_entry_t* entry = map->entries[i];
        if (entry != NULL && entry != MAP_TOMBSTONE)
        {
            return true;
        }
    }

    return false;
}