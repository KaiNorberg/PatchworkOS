#include "page_table.h"

#include <string.h>

#include "pmm/pmm.h"
#include "debug/debug.h"
#include "vmm/vmm.h"
#include "utils/utils.h"
#include "registers/registers.h"

static inline PageEntry page_entry_create(void* address, uint64_t flags)
{
    return ((((uintptr_t)address >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_FLAG_PRESENT);
}

static inline PageTable* page_table_get(PageTable* pageTable, uint64_t index)
{
    PageEntry entry = pageTable->entries[index];

    if (!PAGE_ENTRY_GET_FLAG(entry, PAGE_FLAG_PRESENT))
    {
        return NULL;
    }

    return VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry));
}

static inline PageTable* page_table_get_or_allocate(PageTable* pageTable, uint64_t index, uint64_t flags)
{
    PageEntry entry = pageTable->entries[index];

    if (PAGE_ENTRY_GET_FLAG(entry, PAGE_FLAG_PRESENT))
    {
        return VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry));
    }
    else
    {
        PageTable* address = VMM_LOWER_TO_HIGHER(pmm_allocate());
        memset(address, 0, PAGE_SIZE);

        pageTable->entries[index] = page_entry_create(VMM_HIGHER_TO_LOWER(address), flags);

        return address;
    }
}

static inline void page_table_free_level(PageTable* pageTable, int64_t level)
{
    if (level < 0)
    {
        return;
    }

    for (uint64_t i = 0; i < PAGE_ENTRY_AMOUNT; i++)
    {   
        PageEntry entry = pageTable->entries[i];
        if (!PAGE_ENTRY_GET_FLAG(entry, PAGE_FLAG_PRESENT))
        {
            continue;
        }

        page_table_free_level(VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry)), level - 1);
    }
    
    pmm_free_page(VMM_HIGHER_TO_LOWER(pageTable));
}

PageTable* page_table_new(void)
{
    PageTable* pageTable = VMM_LOWER_TO_HIGHER(pmm_allocate());
    memset(pageTable, 0, PAGE_SIZE);
    
    return pageTable;
}

void page_table_free(PageTable* pageTable)
{    
    page_table_free_level(pageTable, 4);
}

void page_table_load(PageTable* pageTable)
{
    pageTable = VMM_HIGHER_TO_LOWER(pageTable);

    if (CR3_READ() != (uint64_t)pageTable)
    {
        CR3_WRITE(pageTable);
    }
}

void page_table_map_pages(PageTable* pageTable, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        page_table_map(pageTable, (void*)((uint64_t)virtualAddress + page * PAGE_SIZE), (void*)((uint64_t)physicalAddress + page * PAGE_SIZE), flags);
    }
}

void page_table_map(PageTable* pageTable, void* virtualAddress, void* physicalAddress, uint16_t flags)
{        
    if ((uint64_t)virtualAddress % PAGE_SIZE != 0)
    {
        debug_panic("Attempt to map invalid virtual address!");
    }    
    else if ((uint64_t)physicalAddress % PAGE_SIZE != 0)
    {
        debug_panic("Attempt to map invalid physical address!");
    }

    PageTable* level3 = page_table_get_or_allocate(pageTable, PAGE_TABLE_GET_INDEX(virtualAddress, 4), 
        (flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR) & ~PAGE_FLAG_GLOBAL);

    PageTable* level2 = page_table_get_or_allocate(level3, PAGE_TABLE_GET_INDEX(virtualAddress, 3), 
        flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);

    PageTable* level1 = page_table_get_or_allocate(level2, PAGE_TABLE_GET_INDEX(virtualAddress, 2), 
        flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);

    PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtualAddress, 1)];

    if (PAGE_ENTRY_GET_FLAG(*entry, PAGE_FLAG_PRESENT))
    {
        debug_panic("Attempted to map already mapped page");
    }

    *entry = page_entry_create(physicalAddress, flags);
}

void* page_table_physical_address(PageTable* pageTable, void* virtualAddress)
{
    uint64_t offset = ((uint64_t)virtualAddress) % PAGE_SIZE;
    virtualAddress = (void*)round_down((uint64_t)virtualAddress, PAGE_SIZE);

    PageTable* level3 = page_table_get(pageTable, PAGE_TABLE_GET_INDEX(virtualAddress, 4));
    if (level3 == NULL)
    {
        return NULL;
    }

    PageTable* level2 = page_table_get(level3, PAGE_TABLE_GET_INDEX(virtualAddress, 3));
    if (level2 == NULL)
    {
        return NULL;
    }

    PageTable* level1 = page_table_get(level2, PAGE_TABLE_GET_INDEX(virtualAddress, 2));
    if (level1 == NULL)
    {
        return NULL;
    }

    PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtualAddress, 1)];

    if (!PAGE_ENTRY_GET_FLAG(*entry, PAGE_FLAG_PRESENT))
    {
        return NULL;
    }

    return (void*)(((uint64_t)PAGE_ENTRY_GET_ADDRESS(*entry)) + offset);
}

void page_table_change_flags(PageTable* pageTable, void* virtualAddress, uint16_t flags)
{
    if ((uint64_t)virtualAddress % PAGE_SIZE != 0)
    {
        debug_panic("Attempt to change flags of invalid virtual address!");
    }

    PageTable* level3 = page_table_get(pageTable, PAGE_TABLE_GET_INDEX(virtualAddress, 4));
    if (level3 == NULL)
    {
        debug_panic("Failed to change page flags");
    }

    PageTable* level2 = page_table_get(level3, PAGE_TABLE_GET_INDEX(virtualAddress, 3));
    if (level2 == NULL)
    {
        debug_panic("Failed to change page flags");
    }

    PageTable* level1 = page_table_get(level2, PAGE_TABLE_GET_INDEX(virtualAddress, 2));
    if (level1 == NULL)
    {
        debug_panic("Failed to change page flags");
    }

    PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtualAddress, 1)];

    if (!PAGE_ENTRY_GET_FLAG(*entry, PAGE_FLAG_PRESENT))
    {
        debug_panic("Failed to change page flags");
    }

    *entry = page_entry_create(PAGE_ENTRY_GET_ADDRESS(*entry), flags);
}