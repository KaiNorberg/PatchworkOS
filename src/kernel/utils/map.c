#include <kernel/utils/map.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

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
    entry->index = _FAIL;
}

static uint64_t map_find_slot(const map_t* map, const map_key_t* key, bool forInsertion)
{
    uint64_t index = (uint64_t)(key->hash & (map->capacity - 1));
    uint64_t firstTombstone = _FAIL;

    for (uint64_t i = 0; i < map->capacity; i++)
    {
        uint64_t currentIndex = (index + i) & (map->capacity - 1);
        map_entry_t* entry = map->entries[currentIndex];

        if (entry == NULL)
        {
            if (forInsertion && firstTombstone != _FAIL)
            {
                return firstTombstone;
            }
            return currentIndex;
        }

        if (entry == MAP_TOMBSTONE)
        {
            if (forInsertion && firstTombstone == _FAIL)
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

    if (forInsertion && firstTombstone != _FAIL)
    {
        return firstTombstone;
    }

    return _FAIL;
}

static uint64_t map_resize(map_t* map, uint64_t newCapacity)
{
    if (!IS_POW2(newCapacity))
    {
        newCapacity = next_pow2(newCapacity);
    }

    if (newCapacity <= map->capacity)
    {
        return 0;
    }

    map_entry_t** newEntries = calloc(newCapacity, sizeof(map_entry_t*));
    if (newEntries == NULL)
    {
        return _FAIL;
    }

    map_entry_t** oldEntries = map->entries;
    uint64_t oldCapacity = map->capacity;
    uint64_t oldLength = map->length;
    uint64_t oldTombstones = map->tombstones;

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
            if (newIndex == _FAIL)
            {
                free(newEntries);
                map->entries = oldEntries;
                map->capacity = oldCapacity;
                map->length = oldLength;
                map->tombstones = oldTombstones;
                return _FAIL;
            }

            entry->index = newIndex;
            entry->map = map;
            map->entries[newIndex] = entry;
            map->length++;
        }
    }

    free(oldEntries);
    return 0;
}

void map_init(map_t* map)
{
    map->length = 0;
    map->capacity = 0;
    map->tombstones = 0;
    map->entries = NULL;
}

void map_deinit(map_t* map)
{
    free(map->entries);
    map->entries = NULL;
    map->length = 0;
    map->capacity = 0;
    map->tombstones = 0;
}

static uint64_t map_resize_check(map_t* map)
{
    uint64_t currentEntries = map->length + map->tombstones;
    if (currentEntries * 100 >= MAP_MAX_LOAD_PERCENTAGE * map->capacity)
    {
        uint64_t newCapacity = (map->capacity == 0) ? MAP_MIN_CAPACITY : (map->capacity * 2);
        return map_resize(map, newCapacity);
    }
    return 0;
}

uint64_t map_insert(map_t* map, const map_key_t* key, map_entry_t* entry)
{
    if (entry == NULL)
    {
        errno = EINVAL;
        return _FAIL;
    }

    if (map_resize_check(map) == _FAIL)
    {
        errno = ENOMEM;
        return _FAIL;
    }

    uint64_t index = map_find_slot(map, key, true);
    if (index == _FAIL)
    {
        errno = ENOMEM;
        return _FAIL;
    }

    if (map->entries[index] != NULL && map->entries[index] != MAP_TOMBSTONE &&
        map_key_is_equal(&map->entries[index]->key, key))
    {
        errno = EEXIST;
        return _FAIL;
    }

    if (map->entries[index] == MAP_TOMBSTONE)
    {
        map->tombstones--;
    }

    entry->key = *key;
    entry->index = index;
    entry->map = map;

    map->entries[index] = entry;
    map->length++;

    return 0;
}

uint64_t map_replace(map_t* map, const map_key_t* key, map_entry_t* entry)
{
    if (entry == NULL)
    {
        errno = EINVAL;
        return _FAIL;
    }

    if (map_resize_check(map) == _FAIL)
    {
        errno = ENOMEM;
        return _FAIL;
    }

    uint64_t index = map_find_slot(map, key, true);
    if (index == _FAIL)
    {
        errno = ENOMEM;
        return _FAIL;
    }

    if (map->entries[index] == MAP_TOMBSTONE)
    {
        map->tombstones--;
        map->length++;
    }
    else if (map->entries[index] == NULL)
    {
        map->length++;
    }

    entry->key = *key;
    entry->index = index;
    entry->map = map;

    map->entries[index] = entry;

    return 0;
}

map_entry_t* map_get(map_t* map, const map_key_t* key)
{
    uint64_t index = map_find_slot(map, key, false);
    if (index == _FAIL)
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

static void map_remove_index(map_t* map, uint64_t index)
{
    map_entry_t* entry = map->entries[index];
    if (entry == NULL || entry == MAP_TOMBSTONE)
    {
        return;
    }

    map->entries[index] = MAP_TOMBSTONE;
    map->length--;
    map->tombstones++;

    if (map->capacity > MAP_MIN_CAPACITY && map->length * 100 < MAP_MIN_LOAD_PERCENTAGE * map->capacity)
    {
        uint64_t newCapacity = map->capacity / 2;
        if (newCapacity < MAP_MIN_CAPACITY)
        {
            newCapacity = MAP_MIN_CAPACITY;
        }
        map_resize(map, newCapacity); // If we fail here, we can just ignore it
    }

    entry->index = _FAIL;
    entry->map = NULL;
}

map_entry_t* map_get_and_remove(map_t* map, const map_key_t* key)
{
    uint64_t index = map_find_slot(map, key, false);
    if (index == _FAIL)
    {
        return NULL;
    }

    map_entry_t* entry = map->entries[index];
    if (entry == NULL || entry == MAP_TOMBSTONE || !map_key_is_equal(&entry->key, key))
    {
        return NULL;
    }

    map_remove_index(map, index);
    return entry;
}

void map_remove(map_t* map, map_entry_t* entry)
{
    if (entry == NULL || entry->index == _FAIL)
    {
        return;
    }
    assert(entry->map == map);

    map_remove_index(map, entry->index);
}

void map_remove_key(map_t* map, const map_key_t* key)
{
    uint64_t index = map_find_slot(map, key, false);
    if (index == _FAIL)
    {
        return;
    }

    map_entry_t* entry = map->entries[index];
    if (entry == NULL || entry == MAP_TOMBSTONE || !map_key_is_equal(&entry->key, key))
    {
        return;
    }

    map_remove_index(map, index);
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
        map_entry_t* entry = map->entries[i];
        if (entry != NULL && entry != MAP_TOMBSTONE)
        {
            entry->index = _FAIL;
            entry->map = NULL;
        }
        map->entries[i] = NULL;
    }

    map->length = 0;
    map->tombstones = 0;
}

uint64_t map_reserve(map_t* map, uint64_t minCapacity)
{
    if (minCapacity < map->length)
    {
        return 0;
    }

    uint64_t newCapacity = next_pow2(minCapacity);
    if (map_resize(map, newCapacity) == _FAIL)
    {
        errno = ENOMEM;
        return _FAIL;
    }
    return 0;
}
