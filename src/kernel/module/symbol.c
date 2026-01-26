#include <kernel/module/symbol.h>

#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/rwlock.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/proc.h>

static symbol_addr_t** addrArray = NULL;
static size_t addrAmount = 0;
static size_t addrCapacity = 0;

static bool symbol_name_cmp(map_entry_t* entry, const void* key)
{
    symbol_name_t* s = CONTAINER_OF(entry, symbol_name_t, mapEntry);
    return strcmp(s->name, (const char*)key) == 0;
}

static bool symbol_group_cmp(map_entry_t* entry, const void* key)
{
    symbol_group_t* g = CONTAINER_OF(entry, symbol_group_t, mapEntry);
    return g->id == *(const symbol_group_id_t*)key;
}

static MAP_CREATE(nameMap, 1024, symbol_name_cmp);
static MAP_CREATE(groupMap, 64, symbol_group_cmp);

static rwlock_t lock = RWLOCK_CREATE();

symbol_group_id_t symbol_generate_group_id(void)
{
    static _Atomic(symbol_group_id_t) nextGroupId = ATOMIC_VAR_INIT(0);
    return atomic_fetch_add_explicit(&nextGroupId, 1, memory_order_relaxed);
}

// Needed since there could be multiple symbols with the same address, each of these symbols are stored contiguously.
static uint64_t symbol_get_floor_index_for_addr(void* addr)
{
    if (addrArray == NULL || addrAmount == 0)
    {
        return addrAmount;
    }

    uint64_t left = 0;
    uint64_t right = addrAmount - 1;
    uint64_t result = addrAmount;

    while (left <= right)
    {
        uint64_t mid = left + ((right - left) / 2);
        if (addrArray[mid]->addr <= addr)
        {
            result = mid;
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }

    return result;
}

static symbol_addr_t* symbol_insert_address(void* addr, symbol_group_id_t groupId, Elf64_Symbol_Binding binding,
    Elf64_Symbol_Type type, symbol_name_t* symbolName)
{
    uint64_t left = 0;
    uint64_t right = addrAmount;

    while (left < right)
    {
        uint64_t mid = left + ((right - left) / 2);
        if (addrArray[mid]->addr >= addr)
        {
            right = mid;
        }
        else
        {
            left = mid + 1;
        }
    }

    if (addrAmount + 1 > addrCapacity)
    {
        uint64_t newCapacity = addrCapacity == 0 ? 16 : addrCapacity * 2;
        symbol_addr_t** newArray = (symbol_addr_t**)realloc((void*)addrArray, sizeof(symbol_addr_t*) * newCapacity);
        if (newArray == NULL)
        {
            return NULL;
        }
        addrArray = newArray;
        addrCapacity = newCapacity;
    }

    symbol_addr_t* addrEntry = malloc(sizeof(symbol_addr_t));
    if (addrEntry == NULL)
    {
        return NULL;
    }
    list_entry_init(&addrEntry->nameEntry);
    addrEntry->name = symbolName;
    addrEntry->addr = addr;
    addrEntry->groupId = groupId;
    addrEntry->binding = binding;
    addrEntry->type = type;

    uint64_t moveSize = sizeof(symbol_addr_t*) * (addrAmount - left);
    memmove_s((void*)&addrArray[left + 1], moveSize, (void*)&addrArray[left], moveSize);
    addrArray[left] = addrEntry;
    addrAmount++;

    list_push_back(&symbolName->addrs, &addrEntry->nameEntry);
    return addrEntry;
}

static uint64_t symbol_resolve_addr_unlocked(symbol_info_t* outSymbol, void* addr)
{
    if (outSymbol == NULL || addr == NULL)
    {
        errno = EINVAL;
        return _FAIL;
    }

    size_t index = symbol_get_floor_index_for_addr(addr);
    if (index == addrAmount)
    {
        errno = ENOENT;
        return _FAIL;
    }

    symbol_addr_t* addrEntry = addrArray[index];
    symbol_name_t* nameEntry = addrEntry->name;

    strncpy_s(outSymbol->name, SYMBOL_MAX_NAME, nameEntry->name, SYMBOL_MAX_NAME - 1);
    outSymbol->name[SYMBOL_MAX_NAME - 1] = '\0';
    outSymbol->addr = addrEntry->addr;
    outSymbol->groupId = addrEntry->groupId;
    outSymbol->binding = addrEntry->binding;
    outSymbol->type = addrEntry->type;

    return 0;
}

static uint64_t symbol_resolve_name_unlocked(symbol_info_t* outSymbol, const char* name)
{
    if (outSymbol == NULL || name == NULL)
    {
        errno = EINVAL;
        return _FAIL;
    }

    uint64_t hash = hash_buffer(name, strlen(name));
    map_entry_t* entry = map_find(&nameMap, name, hash);
    symbol_name_t* nameEntry = entry ? CONTAINER_OF(entry, symbol_name_t, mapEntry) : NULL;
    if (nameEntry == NULL || list_is_empty(&nameEntry->addrs))
    {
        errno = ENOENT;
        return _FAIL;
    }
    symbol_addr_t* addrEntry = CONTAINER_OF(list_first(&nameEntry->addrs), symbol_addr_t, nameEntry);

    strncpy_s(outSymbol->name, SYMBOL_MAX_NAME, nameEntry->name, SYMBOL_MAX_NAME - 1);
    outSymbol->name[SYMBOL_MAX_NAME - 1] = '\0';
    outSymbol->addr = addrEntry->addr;
    outSymbol->groupId = addrEntry->groupId;
    outSymbol->binding = addrEntry->binding;
    outSymbol->type = addrEntry->type;

    return 0;
}

uint64_t symbol_add(const char* name, void* addr, symbol_group_id_t groupId, Elf64_Symbol_Binding binding,
    Elf64_Symbol_Type type)
{
    if (type != STT_OBJECT && type != STT_FUNC)
    {
        return 0;
    }

    if (name == NULL || addr == NULL)
    {
        errno = EINVAL;
        return _FAIL;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    if (binding == STB_GLOBAL)
    {
        symbol_info_t existingSymbol;
        if (symbol_resolve_name_unlocked(&existingSymbol, name) != _FAIL)
        {
            LOG_DEBUG("global symbol name conflict for '%s'\n", name);
            errno = EEXIST;
            return _FAIL;
        }
    }

    symbol_group_t* symbolGroup = NULL;
    symbol_name_t* symbolName = NULL;
    symbol_addr_t* symbolAddr = NULL;
    bool groupWasCreated = false;
    bool nameWasCreated = false;

    uint64_t groupHash = hash_uint64(groupId);
    map_entry_t* groupEntry = map_find(&groupMap, &groupId, groupHash);
    symbolGroup = groupEntry ? CONTAINER_OF(groupEntry, symbol_group_t, mapEntry) : NULL;
    if (symbolGroup == NULL)
    {
        symbolGroup = malloc(sizeof(symbol_group_t));
        if (symbolGroup == NULL)
        {
            goto error;
        }
        map_entry_init(&symbolGroup->mapEntry);
        list_init(&symbolGroup->names);
        symbolGroup->id = groupId;

        map_insert(&groupMap, &symbolGroup->mapEntry, groupHash);
        groupWasCreated = true;
    }

    uint64_t nameHash = hash_buffer(name, strlen(name));
    map_entry_t* nameEntry = map_find(&nameMap, name, nameHash);
    symbolName = nameEntry ? CONTAINER_OF(nameEntry, symbol_name_t, mapEntry) : NULL;
    if (symbolName == NULL)
    {
        symbolName = malloc(sizeof(symbol_name_t));
        if (symbolName == NULL)
        {
            goto error;
        }
        list_entry_init(&symbolName->groupEntry);
        map_entry_init(&symbolName->mapEntry);
        list_init(&symbolName->addrs);
        strncpy_s(symbolName->name, SYMBOL_MAX_NAME, name, SYMBOL_MAX_NAME - 1);
        symbolName->name[SYMBOL_MAX_NAME - 1] = '\0';

        map_insert(&nameMap, &symbolName->mapEntry, nameHash);
        list_push_back(&symbolGroup->names, &symbolName->groupEntry);
        nameWasCreated = true;
    }

    symbolAddr = symbol_insert_address(addr, groupId, binding, type, symbolName);
    if (symbolAddr == NULL)
    {
        goto error;
    }

    return 0;

error:
    if (symbolAddr == NULL)
    {
        if (symbolName != NULL && nameWasCreated)
        {
            uint64_t nameHash = hash_buffer(symbolName->name, strlen(symbolName->name));
            map_remove(&nameMap, &symbolName->mapEntry, nameHash);
            list_remove(&symbolName->groupEntry);
            free(symbolName);
        }
    }

    if (symbolName == NULL)
    {
        if (symbolGroup != NULL && groupWasCreated)
        {
            uint64_t groupHash = hash_uint64(symbolGroup->id);
            map_remove(&groupMap, &symbolGroup->mapEntry, groupHash);
            free(symbolGroup);
        }
    }

    LOG_DEBUG("failed to add symbol '%s' at address %p (%s)\n", name, addr, strerror(errno));
    return _FAIL;
}

void symbol_remove_group(symbol_group_id_t groupId)
{
    RWLOCK_WRITE_SCOPE(&lock);

    uint64_t groupHash = hash_uint64(groupId);
    map_entry_t* entry = map_find(&groupMap, &groupId, groupHash);
    symbol_group_t* groupEntry = entry ? CONTAINER_OF(entry, symbol_group_t, mapEntry) : NULL;
    if (groupEntry == NULL)
    {
        return;
    }

    while (!list_is_empty(&groupEntry->names))
    {
        symbol_name_t* nameEntry = CONTAINER_OF(list_pop_front(&groupEntry->names), symbol_name_t, groupEntry);
        list_entry_init(&nameEntry->groupEntry);
    }

    size_t writeIdx = 0;
    for (size_t readIdx = 0; readIdx < addrAmount; readIdx++)
    {
        symbol_addr_t* addrEntry = addrArray[readIdx];
        if (addrEntry->groupId == groupId)
        {
            symbol_name_t* nameEntry = addrEntry->name;

            list_remove(&addrEntry->nameEntry);
            free(addrEntry);

            if (list_is_empty(&nameEntry->addrs))
            {
                uint64_t nameHash = hash_buffer(nameEntry->name, strlen(nameEntry->name));
                map_remove(&nameMap, &nameEntry->mapEntry, nameHash);
                list_remove(&nameEntry->groupEntry);
                free(nameEntry);
            }
        }
        else
        {
            if (writeIdx != readIdx)
            {
                addrArray[writeIdx] = addrArray[readIdx];
            }
            writeIdx++;
        }
    }
    addrAmount = writeIdx;

    map_remove(&groupMap, &groupEntry->mapEntry, groupHash);
    free(groupEntry);

    if (addrAmount == 0)
    {
        free((void*)addrArray);
        addrArray = NULL;
        addrCapacity = 0;
    }
    else if (addrArray != NULL && addrAmount < addrCapacity / 4)
    {
        uint64_t newCapacity = addrCapacity / 2;
        symbol_addr_t** newArray = (symbol_addr_t**)realloc((void*)addrArray, sizeof(symbol_addr_t*) * newCapacity);
        if (newArray != NULL)
        {
            addrArray = newArray;
            addrCapacity = newCapacity;
        }
    }
}

uint64_t symbol_resolve_addr(symbol_info_t* outSymbol, void* addr)
{
    RWLOCK_READ_SCOPE(&lock);
    return symbol_resolve_addr_unlocked(outSymbol, addr);
}

uint64_t symbol_resolve_name(symbol_info_t* outSymbol, const char* name)
{
    RWLOCK_READ_SCOPE(&lock);
    return symbol_resolve_name_unlocked(outSymbol, name);
}
