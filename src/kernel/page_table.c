#include "page_table.h"

#include <string.h>

#include "debug.h"
#include "pmm.h"
#include "regs.h"
#include "utils.h"
#include "vmm.h"

static PageEntry page_entry_create(void* address, uint64_t flags)
{
    return ((((uintptr_t)address >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_FLAG_PRESENT);
}

static PageTable* page_table_get(PageTable* table, uint64_t index)
{
    PageEntry entry = table->entries[index];

    if (!(entry & PAGE_FLAG_PRESENT))
    {
        return NULL;
    }

    return VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry));
}

static PageTable* page_table_get_or_allocate(PageTable* table, uint64_t index, uint64_t flags)
{
    PageEntry entry = table->entries[index];

    if (entry & PAGE_FLAG_PRESENT)
    {
        return VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry));
    }
    else
    {
        PageTable* address = VMM_LOWER_TO_HIGHER(pmm_alloc());
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
        if (!(entry & PAGE_FLAG_PRESENT))
        {
            continue;
        }

        if (level != 1 || (entry & PAGE_FLAG_OWNED))
        {
            page_table_free_level(VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry)), level - 1);
        }
    }

    pmm_free(VMM_HIGHER_TO_LOWER(table));
}

PageTable* page_table_new(void)
{
    PageTable* table = VMM_LOWER_TO_HIGHER(pmm_alloc());
    memset(table, 0, PAGE_SIZE);

    return table;
}

void page_table_free(PageTable* table)
{
    // Will also free any pages mapped in the page table
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

void* page_table_phys_addr(PageTable* table, const void* virtAddr)
{
    uint64_t offset = ((uint64_t)virtAddr) % PAGE_SIZE;
    virtAddr = (void*)ROUND_DOWN(virtAddr, PAGE_SIZE);

    PageTable* level3 = page_table_get(table, PAGE_TABLE_GET_INDEX(virtAddr, 4));
    PageTable* level2 = page_table_get(level3, PAGE_TABLE_GET_INDEX(virtAddr, 3));
    PageTable* level1 = page_table_get(level2, PAGE_TABLE_GET_INDEX(virtAddr, 2));
    PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtAddr, 1)];

    return (void*)(((uint64_t)PAGE_ENTRY_GET_ADDRESS(*entry)) + offset);
}

bool page_table_mapped(PageTable* table, const void* virtAddr, uint64_t pageAmount)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        PageTable* level3 = page_table_get(table, PAGE_TABLE_GET_INDEX(virtAddr, 4));
        if (level3 == NULL)
        {
            return false;
        }

        PageTable* level2 = page_table_get(level3, PAGE_TABLE_GET_INDEX(virtAddr, 3));
        if (level2 == NULL)
        {
            return false;
        }

        PageTable* level1 = page_table_get(level2, PAGE_TABLE_GET_INDEX(virtAddr, 2));
        if (level1 == NULL)
        {
            return false;
        }

        PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtAddr, 1)];
        if (!(*entry & PAGE_FLAG_PRESENT))
        {
            return false;
        }

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
    }

    return true;
}

void page_table_map(PageTable* table, void* virtAddr, void* physAddr, uint64_t pageAmount, uint64_t flags)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        PageTable* level3 = page_table_get_or_allocate(
            table, PAGE_TABLE_GET_INDEX(virtAddr, 4), (flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER) & ~PAGE_FLAG_GLOBAL);

        PageTable* level2 = page_table_get_or_allocate(
            level3, PAGE_TABLE_GET_INDEX(virtAddr, 3), flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER);

        PageTable* level1 = page_table_get_or_allocate(
            level2, PAGE_TABLE_GET_INDEX(virtAddr, 2), flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER);

        PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtAddr, 1)];

        *entry = page_entry_create(physAddr, flags);

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
        physAddr = (void*)((uint64_t)physAddr + PAGE_SIZE);
    }
}

void page_table_unmap(PageTable* table, void* virtAddr, uint64_t pageAmount)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        PageTable* level3 = page_table_get(table, PAGE_TABLE_GET_INDEX(virtAddr, 4));
        PageTable* level2 = page_table_get(level3, PAGE_TABLE_GET_INDEX(virtAddr, 3));
        PageTable* level1 = page_table_get(level2, PAGE_TABLE_GET_INDEX(virtAddr, 2));
        PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtAddr, 1)];

        if (*entry & PAGE_FLAG_OWNED)
        {
            pmm_free(PAGE_ENTRY_GET_ADDRESS(*entry));
        }
        *entry = 0;

        PAGE_INVALIDATE(virtAddr);

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
    }
}

void page_table_change_flags(PageTable* table, void* virtAddr, uint64_t pageAmount, uint64_t flags)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        PageTable* level3 = page_table_get(table, PAGE_TABLE_GET_INDEX(virtAddr, 4));
        PageTable* level2 = page_table_get(level3, PAGE_TABLE_GET_INDEX(virtAddr, 3));
        PageTable* level1 = page_table_get(level2, PAGE_TABLE_GET_INDEX(virtAddr, 2));
        PageEntry* entry = &level1->entries[PAGE_TABLE_GET_INDEX(virtAddr, 1)];

        uint64_t finalFlags = flags;
        if (*entry & PAGE_FLAG_OWNED)
        {
            finalFlags |= PAGE_FLAG_OWNED;
        }

        *entry = page_entry_create(PAGE_ENTRY_GET_ADDRESS(*entry), finalFlags);
        PAGE_INVALIDATE(virtAddr);

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
    }
}