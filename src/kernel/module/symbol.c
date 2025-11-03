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
static uint64_t addrAmount = 0;
static uint64_t addrCapacity = 0;

static map_t nameMap = MAP_CREATE;

static map_t groupMap = MAP_CREATE;

static rwlock_t lock = RWLOCK_CREATE;

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
        return ERR;
    }

    uint64_t index = symbol_get_floor_index_for_addr(addr);
    if (index == addrAmount)
    {
        errno = ENOENT;
        return ERR;
    }

    symbol_addr_t* addrEntry = addrArray[index];
    symbol_name_t* nameEntry = CONTAINER_OF(addrEntry->nameEntry.list, symbol_name_t, addrs);

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
        return ERR;
    }

    map_key_t key = map_key_string(name);
    symbol_name_t* nameEntry = CONTAINER_OF_SAFE(map_get(&nameMap, &key), symbol_name_t, mapEntry);
    if (nameEntry == NULL || list_is_empty(&nameEntry->addrs))
    {
        errno = ENOENT;
        return ERR;
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
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    if (binding == STB_GLOBAL)
    {
        symbol_info_t existingSymbol;
        if (symbol_resolve_name_unlocked(&existingSymbol, name) != ERR)
        {
            LOG_DEBUG("global symbol name conflict for '%s'\n", name);
            errno = EEXIST;
            return ERR;
        }
    }

    symbol_group_t* symbolGroup = NULL;
    symbol_name_t* symbolName = NULL;
    symbol_addr_t* symbolAddr = NULL;
    bool groupWasCreated = false;
    bool nameWasCreated = false;

    map_key_t groupKey = map_key_uint64(groupId);
    symbolGroup = CONTAINER_OF_SAFE(map_get(&groupMap, &groupKey), symbol_group_t, entry);
    if (symbolGroup == NULL)
    {
        symbolGroup = malloc(sizeof(symbol_group_t));
        if (symbolGroup == NULL)
        {
            goto error;
        }
        map_entry_init(&symbolGroup->entry);
        list_init(&symbolGroup->names);
        symbolGroup->id = groupId;

        if (map_insert(&groupMap, &groupKey, &symbolGroup->entry) == ERR)
        {
            free(symbolGroup);
            symbolGroup = NULL;
            goto error;
        }
        groupWasCreated = true;
    }

    map_key_t nameKey = map_key_string(name);
    symbolName = CONTAINER_OF_SAFE(map_get(&nameMap, &nameKey), symbol_name_t, mapEntry);
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

        if (map_insert(&nameMap, &nameKey, &symbolName->mapEntry) == ERR)
        {
            free(symbolName);
            symbolName = NULL;
            goto error;
        }
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
            map_key_t key = map_key_string(symbolName->name);
            map_remove(&nameMap, &key);
            list_remove(&symbolGroup->names, &symbolName->groupEntry);
            free(symbolName);
        }
    }

    if (symbolName == NULL)
    {
        if (symbolGroup != NULL && groupWasCreated)
        {
            map_key_t key = map_key_uint64(symbolGroup->id);
            map_remove(&groupMap, &key);
            free(symbolGroup);
        }
    }
    
    LOG_DEBUG("failed to add symbol '%s' at address %p (%s)\n", name, addr, strerror(errno));
    return ERR;
}

void symbol_remove_group(symbol_group_id_t groupId)
{
    RWLOCK_WRITE_SCOPE(&lock);

    map_key_t groupKey = map_key_uint64(groupId);
    symbol_group_t* groupEntry = CONTAINER_OF_SAFE(map_get(&groupMap, &groupKey), symbol_group_t, entry);
    if (groupEntry == NULL)
    {
        return;
    }

    uint64_t writeIdx = 0;
    for (uint64_t readIdx = 0; readIdx < addrAmount; readIdx++)
    {
        if (addrArray[readIdx]->groupId != groupId)
        {
            if (writeIdx != readIdx)
            {
                addrArray[writeIdx] = addrArray[readIdx];
            }
            writeIdx++;
        }
    }
    addrAmount = writeIdx;

    while (!list_is_empty(&groupEntry->names))
    {
        symbol_name_t* nameEntry = CONTAINER_OF(list_pop_first(&groupEntry->names), symbol_name_t, groupEntry);

        while (!list_is_empty(&nameEntry->addrs))
        {
            symbol_addr_t* addrEntry = CONTAINER_OF(list_pop_first(&nameEntry->addrs), symbol_addr_t, nameEntry);
            free(addrEntry);
        }

        map_key_t nameKey = map_key_string(nameEntry->name);
        map_remove(&nameMap, &nameKey);
        free(nameEntry);
    }

    map_remove(&groupMap, &groupKey);
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
