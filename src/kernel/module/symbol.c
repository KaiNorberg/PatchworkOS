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

static map_t nameMap = MAP_CREATE;

static rwlock_t lock = RWLOCK_CREATE;

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

static symbol_addr_t* symbol_insert_address(void* addr)
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

    void* newAddrArray = realloc(addrArray, sizeof(symbol_addr_t*) * (addrAmount + 1));
    if (newAddrArray == NULL)
    {
        return NULL;
    }
    addrArray = newAddrArray;

    symbol_addr_t* addrEntry = malloc(sizeof(symbol_addr_t));
    if (addrEntry == NULL)
    {
        return NULL;
    }
    list_entry_init(&addrEntry->entry);
    addrEntry->addr = addr;

    memmove(&addrArray[left + 1], &addrArray[left], sizeof(symbol_addr_t*) * (addrAmount - left));
    addrArray[left] = addrEntry;
    addrAmount++;
    return addrEntry;
}

static symbol_name_t* symbol_find_or_create_name(const char* name)
{
    map_key_t key = map_key_string(name);
    symbol_name_t* nameEntry = CONTAINER_OF_SAFE(map_get(&nameMap, &key), symbol_name_t, entry);
    if (nameEntry != NULL)
    {
        return nameEntry;
    }

    nameEntry = malloc(sizeof(symbol_name_t));
    if (nameEntry == NULL)
    {
        return NULL;
    }
    map_entry_init(&nameEntry->entry);
    list_init(&nameEntry->addrs);
    strncpy(nameEntry->name, name, SYMBOL_MAX_NAME - 1);
    nameEntry->name[SYMBOL_MAX_NAME - 1] = '\0';

    if (map_insert(&nameMap, &key, &nameEntry->entry) == ERR)
    {
        free(nameEntry);
        return NULL;
    }

    return nameEntry;
}

void symbol_load_kernel_symbols(const boot_kernel_t* kernel)
{
    Elf64_File_Symbol_Iterator it = ELF_FILE_SYMBOL_ITERATOR_CREATE(&kernel->elf);
    while (elf_file_symbol_iterator_next(&it))
    {
        if (it.symbol->st_name == 0 || it.symbol->st_value == 0)
        {
            continue;
        }
        if (symbol_add(it.symbolName, (void*)it.symbol->st_value) == ERR)
        {
            panic(NULL, "Failed to add kernel symbol '%s' at address 0x%llx", it.symbolName, it.symbol->st_value);
        }
    }
    LOG_INFO("Loaded %llu kernel symbols\n", addrAmount);
}

uint64_t symbol_add(const char* name, void* addr)
{
    if (name == NULL || addr == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    symbol_name_t* nameEntry = symbol_find_or_create_name(name);
    if (nameEntry == NULL)
    {
        return ERR;
    }

    symbol_addr_t* addrEntry = symbol_insert_address(addr);
    if (addrEntry == NULL)
    {
        if (list_is_empty(&nameEntry->addrs))
        {
            map_key_t key = map_key_string(nameEntry->name);
            map_remove(&nameMap, &key);
            free(nameEntry);
        }
        return ERR;
    }

    list_push(&nameEntry->addrs, &addrEntry->entry);
    return 0;
}

void symbol_remove_addr(void* addr)
{
    if (addr == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    uint64_t startIndex = symbol_get_floor_index_for_addr(addr);
    if (startIndex == addrAmount || addrArray[startIndex]->addr != addr)
    {
        return;
    }
    uint64_t endIndex = startIndex;
    while (endIndex < addrAmount && addrArray[endIndex]->addr == addr)
    {
        endIndex++;
    }

    for (uint64_t i = startIndex; i < endIndex; i++)
    {
        symbol_addr_t* addrEntry = addrArray[i];
        symbol_name_t* nameEntry = CONTAINER_OF(addrEntry->entry.list, symbol_name_t, addrs);

        list_remove(&nameEntry->addrs, &addrEntry->entry);
        if (list_is_empty(&nameEntry->addrs))
        {
            map_key_t key = map_key_string(nameEntry->name);
            map_remove(&nameMap, &key);
            free(nameEntry);
        }
        free(addrEntry);
    }
    memmove(&addrArray[startIndex], &addrArray[endIndex], sizeof(symbol_addr_t*) * (addrAmount - endIndex));
    addrAmount -= (endIndex - startIndex);
}

void symbol_remove_name(const char* name)
{
    if (name == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    map_key_t key = map_key_string(name);
    symbol_name_t* nameEntry = CONTAINER_OF_SAFE(map_get(&nameMap, &key), symbol_name_t, entry);
    if (nameEntry == NULL)
    {
        return;
    }

    while (!list_is_empty(&nameEntry->addrs))
    {
        symbol_addr_t* addrEntry = CONTAINER_OF(list_pop(&nameEntry->addrs), symbol_addr_t, entry);

        uint64_t index = symbol_get_floor_index_for_addr(addrEntry->addr);
        if (index == addrAmount || addrArray[index]->addr != addrEntry->addr)
        {
            panic(NULL, "Inconsistent symbol table state");
        }
        memmove(&addrArray[index], &addrArray[index + 1], sizeof(symbol_addr_t*) * (addrAmount - index - 1));
        addrAmount--;
        free(addrEntry);
    }

    map_remove(&nameMap, &key);
    free(nameEntry);
}

uint64_t symbol_resolve_addr(symbol_info_t* outSymbol, void* addr)
{
    if (outSymbol == NULL || addr == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_READ_SCOPE(&lock);

    uint64_t index = symbol_get_floor_index_for_addr(addr);
    if (index == addrAmount)
    {
        errno = ENOENT;
        return ERR;
    }

    symbol_addr_t* addrEntry = addrArray[index];
    symbol_name_t* nameEntry = CONTAINER_OF(addrEntry->entry.list, symbol_name_t, addrs);

    strncpy(outSymbol->name, nameEntry->name, SYMBOL_MAX_NAME - 1);
    outSymbol->name[SYMBOL_MAX_NAME - 1] = '\0';
    outSymbol->addr = addrEntry->addr;

    return 0;
}

uint64_t symbol_resolve_name(symbol_info_t* outSymbol, const char* name)
{
    if (outSymbol == NULL || name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_READ_SCOPE(&lock);

    map_key_t key = map_key_string(name);
    symbol_name_t* nameEntry = CONTAINER_OF_SAFE(map_get(&nameMap, &key), symbol_name_t, entry);
    if (nameEntry == NULL || list_is_empty(&nameEntry->addrs))
    {
        errno = ENOENT;
        return ERR;
    }
    symbol_addr_t* addrEntry = CONTAINER_OF(list_first(&nameEntry->addrs), symbol_addr_t, entry);

    strncpy(outSymbol->name, nameEntry->name, SYMBOL_MAX_NAME - 1);
    outSymbol->name[SYMBOL_MAX_NAME - 1] = '\0';
    outSymbol->addr = addrEntry->addr;

    return 0;
}
