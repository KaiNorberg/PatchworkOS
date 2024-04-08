#include "page_table.h"

#include <string.h>

#include "pmm/pmm.h"
#include "debug/debug.h"
#include "vmm/vmm.h"
#include "utils/utils.h"
#include "regs/regs.h"

static PageEntry page_entry_create(void* address, uint64_t flags)
{
    return ((((uintptr_t)address >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_FLAG_PRESENT);
}

static PageTable* page_table_get(PageTable* table, uint64_t index)
{
    PageEntry entry = table->entries[index];

    if (!PAGE_ENTRY_GET_FLAG(entry, PAGE_FLAG_PRESENT))
    {
        return NULL;
    }

    return VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry));
}

static PageTable* page_table_get_or_allocate(PageTable* table, uint64_t index, uint64_t flags)
{
    PageEntry entry = table->entries[index];

    if (PAGE_ENTRY_GET_FLAG(entry, PAGE_FLAG_PRESENT))
    {
        return VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry));
    }
    else
    {
        PageTable* address = VMM_LOWER_TO_HIGHER(pmm_allocate());
        memset(address, 0, PAGE_SIZE);

        table->entries[index] = page_entry_create(VMM_HIGHER_TO_LOWER(address), flags);

        return address;
    }
}

static void page_table_free_level(PageTable* table, int64_t level)
{
    if (level < 0)
    {
        return;
    }

    for (uint64_t i = 0; i < PAGE_ENTRY_AMOUNT; i++)
    {   
        PageEntry entry = table->entries[i];
        if (!PAGE_ENTRY_GET_FLAG(entry, PAGE_FLAG_PRESENT))
        {
            continue;
        }

        page_table_free_level(VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry)), level - 1);
    }
    
    pmm_free(VMM_HIGHER_TO_LOWER(table));
}

PageTable* page_table_new(void)
{
    PageTable* table = VMM_LOWER_TO_HIGHER(pmm_allocate());
    memset(table, 0, PAGE_SIZE);
    
    return table;
}

void page_table_free(PageTable* table)
{    
    //Will also free any pages mapped in the page table
    page_table_free_level(table, 4);
}

void page_table_load(PageTable* table)
{
    table = VMM_HIGHER_TO_LOWER(table);

    if (cr3_read() != (uint64_t)table)
    {
        cr3_write((uint64_t)table);
    }
}

void page_table_map_pages(PageTable* table, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        page_table_map(table, (void*)((uint64_t)virtualAddress + page * PAGE_SIZE), (void*)((uint64_t)physicalAddress + page * PAGE_SIZE), flags);
    }
}

void page_table_map(PageTable* table, void* virtualAddress, void* physicalAddress, uint16_t flags)
{        
    if ((uint64_t)virtualAddress % PAGE_SIZE != 0)
    {
        debug_panic("Failed to map page, invalid virtual address");
    }    
    else if ((uint64_t)physicalAddress % PAGE_SIZE != 0)
    {
        debug_panic("Failed to map page, invalid physical address");
    }

    PageTable* level3 = page_table_get_or_allocate(table, PAGE_TABLE_GET_INDEX(virtualAddress, 4), 
        (flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR) & ~PAGE_FLAG_GLOBAL);

    PageTable* level2 = page_table_get_or_allocate(level3, PAGE_TABLE_GET_INDEX(virtualAddress, 3), 
        flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);

    PageTable* level1 = page_table_get_or_allocate(level2, PAGE_TABLE_GET_INDEX(virtualAddress, 2), 
        flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);

    PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtualAddress, 1)];

    if (PAGE_ENTRY_GET_FLAG(*entry, PAGE_FLAG_PRESENT))
    {
        debug_panic("Failed to map page, already present");
    }

    *entry = page_entry_create(physicalAddress, flags);
}

void page_table_unmap_pages(PageTable* table, void* virtualAddress, uint64_t pageAmount)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        page_table_unmap(table, (void*)((uint64_t)virtualAddress + page * PAGE_SIZE));
    }
}

void page_table_unmap(PageTable* table, void* virtualAddress)
{
    PageTable* level3 = page_table_get(table, PAGE_TABLE_GET_INDEX(virtualAddress, 4));
    if (level3 == NULL)
    {
        debug_panic("Failed to unmap page");
    }

    PageTable* level2 = page_table_get(level3, PAGE_TABLE_GET_INDEX(virtualAddress, 3));
    if (level2 == NULL)
    {
        debug_panic("Failed to unmap page");
    }

    PageTable* level1 = page_table_get(level2, PAGE_TABLE_GET_INDEX(virtualAddress, 2));
    if (level1 == NULL)
    {
        debug_panic("Failed to unmap page");
    }

    PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtualAddress, 1)];
    if (!PAGE_ENTRY_GET_FLAG(*entry, PAGE_FLAG_PRESENT))
    {
        debug_panic("Failed to unmap page");
    }

    *entry = 0;
}

void* page_table_physical_address(PageTable* table, const void* virtualAddress)
{
    uint64_t offset = ((uint64_t)virtualAddress) % PAGE_SIZE;
    virtualAddress = (void*)ROUND_DOWN(virtualAddress, PAGE_SIZE);

    PageTable* level3 = page_table_get(table, PAGE_TABLE_GET_INDEX(virtualAddress, 4));
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

void page_table_change_flags(PageTable* table, void* virtualAddress, uint16_t flags)
{
    if ((uint64_t)virtualAddress % PAGE_SIZE != 0)
    {
        debug_panic("Failed to change page flags");
    }

    PageTable* level3 = page_table_get(table, PAGE_TABLE_GET_INDEX(virtualAddress, 4));
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