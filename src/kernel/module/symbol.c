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
static map_t fromNameMap = MAP_CREATE;
static rwlock_t lock = RWLOCK_CREATE;

void symbol_load_kernel_symbols(const boot_kernel_t* kernel)
{
    Elf64_File_Symbol_Iterator it = ELF_FILE_SYMBOL_ITERATOR_CREATE(&kernel->elf);
    while (elf_file_symbol_iterator_next(&it))
    {
        if (it.symbol->st_name == 0 || it.symbol->st_value == 0 || ELF64_ST_BIND(it.symbol->st_info) == STB_LOCAL)
        {
            continue;
        }
        if (symbol_add(it.symbolName, (void*)it.symbol->st_value) == ERR)
        {
            panic(NULL, "Failed to add kernel symbol");
        }
    }
}

uint64_t symbol_add(const char* name, void* addr)
{
    if (name == NULL || addr == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    symbol_name_t* nameEntry = malloc(sizeof(symbol_name_t));
    if (nameEntry == NULL)
    {
        return ERR;
    }
    map_entry_init(&nameEntry->fromNameEntry);
    nameEntry->addr = addr;

    map_key_t key = map_key_string(name);
    if (map_insert(&fromNameMap, &key, &nameEntry->fromNameEntry) == ERR)
    {
        free(nameEntry);
        return ERR;
    }

    symbol_addr_t* newFromAddrArray = realloc(fromAddrArray, sizeof(symbol_addr_t) * (fromAddrAmount + 1));
    if (newFromAddrArray == NULL)
    {
        map_remove(&fromNameMap, &key);
        free(nameEntry);
        return ERR;
    }
    fromAddrArray = newFromAddrArray;

    for (uint64_t i = fromAddrAmount; i > 0; i--)
    {
        if (fromAddrArray[i - 1].addr > addr)
        {
            fromAddrArray[i] = fromAddrArray[i - 1];
        }
        else
        {
            fromAddrAmount++;
            fromAddrArray[i].addr = addr;
            strncpy_s(fromAddrArray[i].name, SYMBOL_MAX_NAME, name, SYMBOL_MAX_NAME - 1);
            return 0;
        }
    }

    fromAddrAmount++;
    fromAddrArray[0].addr = addr;
    strncpy_s(fromAddrArray[0].name, SYMBOL_MAX_NAME, name, SYMBOL_MAX_NAME - 1);

    return 0;
}

static symbol_addr_t* symbol_get_entry_for_addr(void* addr)
{
    if (fromAddrArray == NULL || fromAddrAmount == 0)
    {
        return NULL;
    }

    int64_t left = 0;
    int64_t right = fromAddrAmount - 1;
    while (left <= right)
    {
        int64_t mid = left + (right - left) / 2;
        if (fromAddrArray[mid].addr == addr)
        {
            return &fromAddrArray[mid];
        }
        else if (fromAddrArray[mid].addr < addr)
        {
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }

    if (right < 0)
    {
        return NULL;
    }

    return &fromAddrArray[right];
}

void symbol_remove_addr(void* addr)
{
    if (addr == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    symbol_addr_t* entry = symbol_get_entry_for_addr(addr);
    if (entry == NULL || entry->addr != addr)
    {
        return;
    }

    map_key_t key = map_key_string(entry->name);
    map_remove(&fromNameMap, &key);

    uint64_t index = entry - fromAddrArray;
    memmove(&fromAddrArray[index], &fromAddrArray[index + 1], sizeof(symbol_addr_t) * (fromAddrAmount - index - 1));
    fromAddrAmount--;
}

void symbol_remove_name(const char* name)
{
    if (name == NULL)
    {
        return;
    }

    RWLOCK_WRITE_SCOPE(&lock);

    map_key_t key = map_key_string(name);
    map_entry_t* entry = map_get(&fromNameMap, &key);
    if (entry == NULL)
    {
        return;
    }

    symbol_name_t* nameEntry = CONTAINER_OF(entry, symbol_name_t, fromNameEntry);
    void* addr = nameEntry->addr;

    free(nameEntry);
    map_remove(&fromNameMap, &key);

    symbol_addr_t* addrEntry = symbol_get_entry_for_addr(addr);
    if (addrEntry == NULL || addrEntry->addr != addr)
    {
        return;
    }

    uint64_t index = addrEntry - fromAddrArray;
    memmove(&fromAddrArray[index], &fromAddrArray[index + 1], sizeof(symbol_addr_t) * (fromAddrAmount - index - 1));
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

    symbol_addr_t* entry = symbol_get_entry_for_addr(addr);
    if (entry == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

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
    map_entry_t* entry = map_get(&fromNameMap, &key);
    if (entry == NULL)
    {
        errno = ENOENT;
        return ERR;
    }

    symbol_name_t* nameEntry = CONTAINER_OF(entry, symbol_name_t, fromNameEntry);
    outSymbol->addr = nameEntry->addr;
    strncpy_s(outSymbol->name, SYMBOL_MAX_NAME, name, SYMBOL_MAX_NAME - 1);
    return 0;
}
