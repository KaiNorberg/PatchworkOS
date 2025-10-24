#include "map.h"

#include "mem/heap.h"

#include <errno.h>
#include <string.h>
#include <sys/math.h>

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

uint64_t hash_object(const void* object, uint64_t length)
{
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;

    for (uint64_t i = 0; i < length; ++i)
    {
        hash ^= (uint64_t)((uint8_t*)object)[i];
        hash *= prime;
    }

    return hash;
}

bool map_key_is_equal(const map_key_t* a, const map_key_t* b)
{
    if (a->hash != b->hash)
    {
        return false;
    }

    if (a->len != b->len)
    {
        return false;
    }

    return memcmp(a->key, b->key, a->len) == 0;
}

void map_entry_init(map_entry_t* entry)
{
    memset(entry->key.key, 0, sizeof(entry->key.key));
    entry->key.len = 0;
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

    if (forInsertion && firstTombstone != ERR)
    {
        return firstTombstone;
    }

    return ERR;
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
    map->capacity = 0;
    map->tombstones = 0;
    map->entries = NULL;

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
        errno = EINVAL;
        return ERR;
    }

    uint32_t currentEntries = map->length + map->tombstones;
    if (currentEntries * 100 >= MAP_MAX_LOAD_PERCENTAGE * map->capacity)
    {
        if (map_resize(map, map->capacity + 1) == ERR)
        {
            errno = ENOMEM;
            return ERR;
        }
    }

    uint64_t index = map_find_slot(map, key, true);
    if (index == ERR)
    {
        errno = ENOMEM;
        return ERR;
    }

    if (map->entries[index] != NULL && map->entries[index] != MAP_TOMBSTONE &&
        map_key_is_equal(&map->entries[index]->key, key))
    {
        errno = EEXIST;
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

    if (map_key_is_equal(&entry->key, key))
    {
        return entry;
    }

    return NULL;
}

void map_remove(map_t* map, const map_key_t* key)
{
    uint64_t index = map_find_slot(map, key, false);
    if (index == ERR)
    {
        return;
    }

    map_entry_t* entry = map->entries[index];
    if (entry == NULL || entry == MAP_TOMBSTONE || !map_key_is_equal(&entry->key, key))
    {
        return;
    }

    map->entries[index] = MAP_TOMBSTONE;
    map->length--;
    map->tombstones++;

    return;
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
    if (map_resize(map, newCapacity) == ERR)
    {
        errno = ENOMEM;
        return ERR;
    }
    return 0;
}
