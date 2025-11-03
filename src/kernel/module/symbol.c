#include <kernel/module/symbol.h>

#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/rwlock.h>

#include <errno.h>
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

static symbol_group_t* symbol_find_or_create_group(symbol_group_id_t groupId)
{
    map_key_t key = map_key_uint64(groupId);
    symbol_group_t* groupEntry = CONTAINER_OF_SAFE(map_get(&groupMap, &key), symbol_group_t, entry);
    if (groupEntry != NULL)
    {
        return groupEntry;
    }

    groupEntry = malloc(sizeof(symbol_group_t));
    if (groupEntry == NULL)
    {
        return NULL;
    }
    map_entry_init(&groupEntry->entry);
    list_init(&groupEntry->names);
    groupEntry->id = groupId;

    if (map_insert(&groupMap, &key, &groupEntry->entry) == ERR)
    {
        free(groupEntry);
        return NULL;
    }

    return groupEntry;
}

static symbol_name_t* symbol_find_or_create_name(const char* name, symbol_group_t* symbolGroup)
{
    map_key_t key = map_key_string(name);
    symbol_name_t* nameEntry = CONTAINER_OF_SAFE(map_get(&nameMap, &key), symbol_name_t, mapEntry);
    if (nameEntry != NULL)
    {
        return nameEntry;
    }

    nameEntry = malloc(sizeof(symbol_name_t));
    if (nameEntry == NULL)
    {
        return NULL;
    }
    list_entry_init(&nameEntry->groupEntry);
    map_entry_init(&nameEntry->mapEntry);
    list_init(&nameEntry->addrs);
    strncpy(nameEntry->name, name, SYMBOL_MAX_NAME - 1);
    nameEntry->name[SYMBOL_MAX_NAME - 1] = '\0';

    if (map_insert(&nameMap, &key, &nameEntry->mapEntry) == ERR)
    {
        free(nameEntry);
        return NULL;
    }

    list_push(&symbolGroup->names, &nameEntry->groupEntry);
    return nameEntry;
}

// Needed since there could be multiple symbols with the same address, each of theses symbols are stored contiguously.
static int64_t symbol_get_floor_index_for_addr(void* addr)
{
    if (addrArray == NULL || addrAmount == 0)
    {
        return addrAmount;
    }

    int64_t left = 0;
    int64_t right = addrAmount - 1;
    int64_t result = addrAmount;

    while (left <= right)
    {
        int64_t mid = left + (right - left) / 2;
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
        uint64_t mid = left + (right - left) / 2;
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
        symbol_addr_t** newArray = realloc(addrArray, sizeof(symbol_addr_t*) * newCapacity);
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

    memmove(&addrArray[left + 1], &addrArray[left], sizeof(symbol_addr_t*) * (addrAmount - left));
    addrArray[left] = addrEntry;
    addrAmount++;

    list_push(&symbolName->addrs, &addrEntry->nameEntry);
    return addrEntry;
}

void symbol_load_kernel_symbols(const boot_kernel_t* kernel)
{
    symbol_group_id_t kernelGroupId = symbol_generate_group_id();
    assert(kernelGroupId == 0);

    const Elf64_File* elf = &kernel->elf;
    uint64_t index = 0;
    while (true)
    {
        Elf64_Sym* sym = elf64_get_symbol_by_index(elf, index++);
        if (sym == NULL)
        {
            break;
        }

        const char* symName = elf64_get_symbol_name(elf, sym);
        void* symAddr = (void*)(uintptr_t)sym->st_value;
        Elf64_Symbol_Binding binding = ELF64_ST_BIND(sym->st_info);
        Elf64_Symbol_Type type = ELF64_ST_TYPE(sym->st_info);
        if (symbol_add(symName, symAddr, kernelGroupId, binding, type) == ERR)
        {
            panic(NULL, "Failed to load kernel symbol '%s' (%s)", symName, strerror(errno));
        }
    }

    LOG_INFO("Loaded %llu kernel symbols\n", addrAmount);
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

    strncpy(outSymbol->name, nameEntry->name, SYMBOL_MAX_NAME - 1);
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

    strncpy(outSymbol->name, nameEntry->name, SYMBOL_MAX_NAME - 1);
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

    symbol_name_t* symbolName = NULL;
    symbol_group_t* symbolGroup = NULL;

    symbolGroup = symbol_find_or_create_group(groupId);
    if (symbolGroup == NULL)
    {
        goto error;
    }

    symbolName = symbol_find_or_create_name(name, symbolGroup);
    if (symbolName == NULL)
    {
        goto error;
    }

    symbol_addr_t* symbolAddr = symbol_insert_address(addr, groupId, binding, type, symbolName);
    if (symbolAddr == NULL)
    {
        goto error;
    }

    return 0;

error:
    if (symbolGroup != NULL && list_length(&symbolGroup->names) <= 1)
    {
        if (list_length(&symbolGroup->names) == 1)
        {
            symbol_name_t* entry = CONTAINER_OF(list_pop(&symbolGroup->names), symbol_name_t, groupEntry);
            assert(entry == symbolName);
        }
        map_key_t key = map_key_uint64(symbolGroup->id);
        map_remove(&groupMap, &key);
        free(symbolGroup);
    }
    if (symbolName != NULL && list_is_empty(&symbolName->addrs))
    {
        map_key_t key = map_key_string(symbolName->name);
        map_remove(&nameMap, &key);
        free(symbolName);
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

    while (!list_is_empty(&groupEntry->names))
    {
        symbol_name_t* nameEntry = CONTAINER_OF(list_pop(&groupEntry->names), symbol_name_t, groupEntry);

        while (!list_is_empty(&nameEntry->addrs))
        {
            symbol_addr_t* addrEntry = CONTAINER_OF(list_pop(&nameEntry->addrs), symbol_addr_t, nameEntry);

            uint64_t index = symbol_get_floor_index_for_addr(addrEntry->addr);
            while (index < addrAmount && addrArray[index]->addr == addrEntry->addr)
            {
                if (addrArray[index] == addrEntry)
                {
                    break;
                }
                index++;
            }

            if (index < addrAmount)
            {
                memmove(&addrArray[index], &addrArray[index + 1], sizeof(symbol_addr_t*) * (addrAmount - index - 1));
                addrAmount--;
            }

            if (addrAmount == 0)
            {
                free(addrArray);
                addrArray = NULL;
                addrCapacity = 0;
            }

            if (addrArray != NULL && addrAmount < addrCapacity / 4)
            {
                uint64_t newCapacity = addrCapacity / 2;
                symbol_addr_t** newArray = realloc(addrArray, sizeof(symbol_addr_t*) * newCapacity);
                if (newArray != NULL) // Ignore failure
                {
                    addrArray = newArray;
                    addrCapacity = newCapacity;
                }
            }

            free(addrEntry);
        }

        map_key_t nameKey = map_key_string(nameEntry->name);
        map_remove(&nameMap, &nameKey);
        free(nameEntry);
    }

    map_remove(&groupMap, &groupKey);
    free(groupEntry);
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
