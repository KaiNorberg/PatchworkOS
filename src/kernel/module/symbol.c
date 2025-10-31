#include <kernel/module/symbol.h>

#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/rwlock.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/proc.h>

static symbol_addr_t* fromAddrArray = NULL;
static uint64_t fromAddrAmount = 0;

static map_t fromGlobalNameMap = MAP_CREATE;

static list_t fromStaticNameList = LIST_CREATE(fromStaticNameList);

static rwlock_t lock = RWLOCK_CREATE;

// Needed since there could be multiple symbols with the same address, each of theses symbols are stored contiguously.
static int64_t symbol_get_floor_index_for_addr(void* addr)
{
    if (fromAddrArray == NULL || fromAddrAmount == 0)
    {
        return fromAddrAmount;
    }

    int64_t left = 0;
    int64_t right = fromAddrAmount - 1;
    int64_t result = fromAddrAmount;

    while (left <= right)
    {
        int64_t mid = left + (right - left) / 2;
        if (fromAddrArray[mid].addr <= addr)
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

static uint64_t symbol_get_insertion_index_for_addr(void* addr)
{
    uint64_t left = 0;
    uint64_t right = fromAddrAmount;

    while (left < right)
    {
        uint64_t mid = left + (right - left) / 2;
        if (fromAddrArray[mid].addr >= addr)
        {
            right = mid;
        }
        else
        {
            left = mid + 1;
        }
    }

    return left;
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
        bool isLocal = ELF64_ST_BIND(it.symbol->st_info) == STB_LOCAL;
        if (symbol_add(it.symbolName, (void*)it.symbol->st_value, isLocal ? SYMBOL_FLAG_STATIC : SYMBOL_FLAG_GLOBAL) == ERR)
        {
            panic(NULL, "Failed to add kernel symbol '%s' at address 0x%llx", it.symbolName, it.symbol->st_value);
        }
    }
}

uint64_t symbol_add(const char* name, void* addr, symbol_flags_t flags)
{
    if (name == NULL || addr == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    symbol_addr_t* newFromAddrArray = realloc(fromAddrArray, sizeof(symbol_addr_t) * (fromAddrAmount + 1));
    if (newFromAddrArray == NULL)
    {
        return ERR;
    }
    fromAddrArray = newFromAddrArray;

    if (flags & SYMBOL_FLAG_STATIC)
    {
        symbol_static_name_t* nameEntry = malloc(sizeof(symbol_name_t));
        if (nameEntry == NULL)
        {
            return ERR;
        }
        list_entry_init(&nameEntry->listEntry);
        strncpy_s(nameEntry->name, SYMBOL_MAX_NAME, name, SYMBOL_MAX_NAME - 1);
        nameEntry->addr = addr;
        list_push(&fromStaticNameList, &nameEntry->listEntry);
    }
    else
    {
        symbol_name_t* nameEntry = malloc(sizeof(symbol_name_t));
        if (nameEntry == NULL)
        {
            return ERR;
        }
        map_entry_init(&nameEntry->fromNameEntry);
        nameEntry->addr = addr;

        map_key_t key = map_key_string(name);
        if (map_insert(&fromGlobalNameMap, &key, &nameEntry->fromNameEntry) == ERR)
        {
            free(nameEntry);
            return ERR;
        }
    }


    uint64_t insertIndex = symbol_get_insertion_index_for_addr(addr);
    memmove(&fromAddrArray[insertIndex + 1], &fromAddrArray[insertIndex],
        sizeof(symbol_addr_t) * (fromAddrAmount - insertIndex));
    fromAddrArray[insertIndex].addr = addr;
    strncpy_s(fromAddrArray[insertIndex].name, SYMBOL_MAX_NAME, name, SYMBOL_MAX_NAME - 1);
    fromAddrAmount++;

    return 0;
}

void symbol_remove_addr(void* addr)
{
    if (addr == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    int64_t startIndex = symbol_get_floor_index_for_addr(addr);
    if (startIndex >= (int64_t)fromAddrAmount)
    {
        return;
    }
    uint64_t endIndex = (uint64_t)startIndex;
    while (endIndex + 1 < fromAddrAmount && fromAddrArray[endIndex + 1].addr == addr)
    {
        endIndex++;
    }

    for (uint64_t i = startIndex; i <= endIndex; i++)
    {
        map_key_t key = map_key_string(fromAddrArray[i].name);
        map_entry_t* entry = map_get(&fromGlobalNameMap, &key);
        if (entry != NULL)
        {
            symbol_name_t* nameEntry = CONTAINER_OF(entry, symbol_name_t, fromNameEntry);
            free(nameEntry);
            map_remove(&fromGlobalNameMap, &key);
        }
    }

    uint64_t removeCount = endIndex - (uint64_t)startIndex + 1;
    memmove(&fromAddrArray[startIndex], &fromAddrArray[endIndex + 1],
        sizeof(symbol_addr_t) * (fromAddrAmount - endIndex - 1));
    fromAddrAmount -= removeCount;
}

void symbol_remove_name(const char* name)
{
    if (name == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    map_key_t key = map_key_string(name);
    map_entry_t* entry = map_get(&fromGlobalNameMap, &key);
    if (entry == NULL)
    {
        return;
    }

    symbol_name_t* nameEntry = CONTAINER_OF(entry, symbol_name_t, fromNameEntry);
    void* addr = nameEntry->addr;

    free(nameEntry);
    map_remove(&fromGlobalNameMap, &key);

    int64_t startIndex = symbol_get_floor_index_for_addr(addr);
    if (startIndex >= (int64_t)fromAddrAmount)
    {
        panic(NULL, "Inconsistent symbol table state");
    }

    uint64_t actualIndex = (uint64_t)startIndex;
    while (actualIndex < fromAddrAmount && fromAddrArray[actualIndex].addr == addr)
    {
        if (strncmp(fromAddrArray[actualIndex].name, name, SYMBOL_MAX_NAME) == 0)
        {
            break;
        }
        actualIndex++;
    }

    if (actualIndex >= fromAddrAmount || fromAddrArray[actualIndex].addr != addr)
    {
        panic(NULL, "Inconsistent symbol table state");
    }

    memmove(&fromAddrArray[actualIndex], &fromAddrArray[actualIndex + 1],
        sizeof(symbol_addr_t) * (fromAddrAmount - actualIndex - 1));
    fromAddrAmount--;
}

uint64_t symbol_resolve_addr(symbol_info_t* outSymbol, void* addr)
{
    if (outSymbol == NULL || addr == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_READ_SCOPE(&lock);

    int64_t index = symbol_get_floor_index_for_addr(addr);
    if (index >= (int64_t)fromAddrAmount)
    {
        errno = ENOENT;
        return ERR;
    }

    symbol_addr_t* entry = &fromAddrArray[index];
    outSymbol->addr = entry->addr; // Might not be exactly equal to addr
    strncpy_s(outSymbol->name, SYMBOL_MAX_NAME, entry->name, SYMBOL_MAX_NAME - 1);
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
    map_entry_t* entry = map_get(&fromGlobalNameMap, &key);
    if (entry == NULL)
    {
        symbol_static_name_t* entry;
        LIST_FOR_EACH(entry, &fromStaticNameList, listEntry)
        {
            if (strncmp(entry->name, name, SYMBOL_MAX_NAME) == 0)
            {
                outSymbol->addr = entry->addr;
                strncpy_s(outSymbol->name, SYMBOL_MAX_NAME, name, SYMBOL_MAX_NAME - 1);
                outSymbol->flags = SYMBOL_FLAG_STATIC;
                return 0;
            }
        }
        errno = ENOENT;
        return ERR;
    }

    symbol_name_t* nameEntry = CONTAINER_OF(entry, symbol_name_t, fromNameEntry);
    outSymbol->addr = nameEntry->addr;
    strncpy_s(outSymbol->name, SYMBOL_MAX_NAME, name, SYMBOL_MAX_NAME - 1);
    outSymbol->flags = SYMBOL_FLAG_GLOBAL;
    return 0;
}
