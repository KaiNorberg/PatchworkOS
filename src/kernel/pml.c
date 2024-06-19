#include "pml.h"

#include <string.h>

#include "debug.h"
#include "pmm.h"
#include "regs.h"
#include "utils.h"
#include "vmm.h"

static pml_entry_t page_entry_create(void* address, uint64_t flags)
{
    return ((((uintptr_t)address >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_PRESENT);
}

static pml_t* pml_get(pml_t* table, uint64_t index)
{
    pml_entry_t entry = table->entries[index];

    if (!(entry & PAGE_PRESENT))
    {
        return NULL;
    }

    return VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry));
}

static pml_t* pml_get_or_allocate(pml_t* table, uint64_t index, uint64_t flags)
{
    pml_entry_t entry = table->entries[index];

    if (entry & PAGE_PRESENT)
    {
        return VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry));
    }
    else
    {
        pml_t* address = VMM_LOWER_TO_HIGHER(pmm_alloc());
        memset(address, 0, PAGE_SIZE);

        table->entries[index] = page_entry_create(VMM_HIGHER_TO_LOWER(address), flags);

        return address;
    }
}

static void pml_free_level(pml_t* table, int64_t level)
{
    if (level < 0)
    {
        return;
    }

    for (uint64_t i = 0; i < PAGE_ENTRY_AMOUNT; i++)
    {
        pml_entry_t entry = table->entries[i];
        if (!(entry & PAGE_PRESENT))
        {
            continue;
        }

        if (level != 1 || (entry & PAGE_OWNED))
        {
            pml_free_level(VMM_LOWER_TO_HIGHER(PAGE_ENTRY_GET_ADDRESS(entry)), level - 1);
        }
    }

    pmm_free(VMM_HIGHER_TO_LOWER(table));
}

pml_t* pml_new(void)
{
    pml_t* table = VMM_LOWER_TO_HIGHER(pmm_alloc());
    memset(table, 0, PAGE_SIZE);

    return table;
}

void pml_free(pml_t* table)
{
    // Will also free any pages mapped in the page table
    pml_free_level(table, 4);
}

void pml_load(pml_t* table)
{
    table = VMM_HIGHER_TO_LOWER(table);

    if (cr3_read() != (uint64_t)table)
    {
        cr3_write((uint64_t)table);
    }
}

void* pml_phys_addr(pml_t* table, const void* virtAddr)
{
    uint64_t offset = ((uint64_t)virtAddr) % PAGE_SIZE;
    virtAddr = (void*)ROUND_DOWN(virtAddr, PAGE_SIZE);

    pml_t* level3 = pml_get(table, PML_GET_INDEX(virtAddr, 4));
    pml_t* level2 = pml_get(level3, PML_GET_INDEX(virtAddr, 3));
    pml_t* level1 = pml_get(level2, PML_GET_INDEX(virtAddr, 2));
    pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];

    return (void*)(((uint64_t)PAGE_ENTRY_GET_ADDRESS(*entry)) + offset);
}

bool pml_mapped(pml_t* table, const void* virtAddr, uint64_t pageAmount)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        pml_t* level3 = pml_get(table, PML_GET_INDEX(virtAddr, 4));
        if (level3 == NULL)
        {
            return false;
        }

        pml_t* level2 = pml_get(level3, PML_GET_INDEX(virtAddr, 3));
        if (level2 == NULL)
        {
            return false;
        }

        pml_t* level1 = pml_get(level2, PML_GET_INDEX(virtAddr, 2));
        if (level1 == NULL)
        {
            return false;
        }

        pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];
        if (!(*entry & PAGE_PRESENT))
        {
            return false;
        }

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
    }

    return true;
}

void pml_map(pml_t* table, void* virtAddr, void* physAddr, uint64_t pageAmount, uint64_t flags)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        pml_t* level3 = pml_get_or_allocate(table, PML_GET_INDEX(virtAddr, 4), (flags | PAGE_WRITE | PAGE_USER) & ~PAGE_GLOBAL);

        pml_t* level2 = pml_get_or_allocate(level3, PML_GET_INDEX(virtAddr, 3), flags | PAGE_WRITE | PAGE_USER);

        pml_t* level1 = pml_get_or_allocate(level2, PML_GET_INDEX(virtAddr, 2), flags | PAGE_WRITE | PAGE_USER);

        pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];

        *entry = page_entry_create(physAddr, flags);

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
        physAddr = (void*)((uint64_t)physAddr + PAGE_SIZE);
    }
}

void pml_unmap(pml_t* table, void* virtAddr, uint64_t pageAmount)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        pml_t* level3 = pml_get(table, PML_GET_INDEX(virtAddr, 4));
        pml_t* level2 = pml_get(level3, PML_GET_INDEX(virtAddr, 3));
        pml_t* level1 = pml_get(level2, PML_GET_INDEX(virtAddr, 2));
        pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];

        if (*entry & PAGE_OWNED)
        {
            pmm_free(PAGE_ENTRY_GET_ADDRESS(*entry));
        }
        *entry = 0;

        PAGE_INVALIDATE(virtAddr);

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
    }
}

void pml_change_flags(pml_t* table, void* virtAddr, uint64_t pageAmount, uint64_t flags)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        pml_t* level3 = pml_get(table, PML_GET_INDEX(virtAddr, 4));
        pml_t* level2 = pml_get(level3, PML_GET_INDEX(virtAddr, 3));
        pml_t* level1 = pml_get(level2, PML_GET_INDEX(virtAddr, 2));
        pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];

        uint64_t finalFlags = flags;
        if (*entry & PAGE_OWNED)
        {
            finalFlags |= PAGE_OWNED;
        }

        *entry = page_entry_create(PAGE_ENTRY_GET_ADDRESS(*entry), finalFlags);
        PAGE_INVALIDATE(virtAddr);

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
    }
}
