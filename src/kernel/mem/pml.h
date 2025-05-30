#pragma once

#include "cpu/regs.h"
#include "defs.h"

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITE (1 << 1)
#define PAGE_USER (1 << 2)
#define PAGE_WRITE_TROUGH (1 << 3)
#define PAGE_CACHE_DISABLED (1 << 4)
#define PAGE_ACCESSED (1 << 5)
#define PAGE_DIRTY (1 << 6)
#define PAGE_PAGE_SIZE (1 << 7)
#define PAGE_GLOBAL (1 << 8)

// If the page is owned by the page table and should be freed when the page is unmapped.
#define PAGE_OWNED (1 << 9)

#define PAGE_ENTRY_AMOUNT 512
#define PAGE_ENTRY_GET_FLAGS(entry) ((entry) & ~0x000FFFFFFFFFF000)
#define PAGE_ENTRY_GET_ADDRESS(entry) VMM_LOWER_TO_HIGHER((entry) & 0x000FFFFFFFFFF000)

#define PAGE_INVALIDATE(address) asm volatile("invlpg %0" : : "m"(address))

/*#define PML_GET_INDEX(address, level) \
    (((uint64_t)(address) & ((uint64_t)0x1FF << (((level) - 1) * 9 + 12))) >> (((level) - 1) * 9 + 12))*/
#define PML_GET_INDEX(address, level) (((uint64_t)(address) >> (((level) - 1) * 9 + 12)) & 0x1FF)

typedef uint64_t pml_entry_t;

typedef struct pml
{
    pml_entry_t entries[PAGE_ENTRY_AMOUNT];
} pml_t;

static pml_entry_t page_entry_create(void* physAddr, uint64_t flags)
{
    return ((((uintptr_t)physAddr >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_PRESENT);
}

static pml_t* pml_get(pml_t* table, uint64_t index)
{
    pml_entry_t entry = table->entries[index];
    if (!(entry & PAGE_PRESENT))
    {
        return NULL;
    }
    return PAGE_ENTRY_GET_ADDRESS(entry);
}

static pml_t* pml_get_or_alloc(pml_t* table, uint64_t index, uint64_t flags)
{
    pml_entry_t entry = table->entries[index];
    if (entry & PAGE_PRESENT)
    {
        return PAGE_ENTRY_GET_ADDRESS(entry);
    }
    else
    {
        pml_t* address = pmm_alloc();
        if (address == NULL)
        {
            return NULL;
        }
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
            pml_free_level(PAGE_ENTRY_GET_ADDRESS(entry), level - 1);
        }
    }

    pmm_free(table);
}

static inline pml_t* pml_new(void)
{
    pml_t* table = pmm_alloc();
    if (table == NULL)
    {
        return NULL;
    }

    memset(table, 0, PAGE_SIZE);
    return table;
}

static inline void pml_free(pml_t* table)
{
    pml_free_level(table, 4);
}

static inline void pml_load(pml_t* table)
{
    uint64_t cr3 = (uint64_t)VMM_HIGHER_TO_LOWER(table);
    if (cr3_read() != cr3)
    {
        cr3_write(cr3);
    }
}

static inline void* pml_phys_addr(pml_t* table, const void* virtAddr)
{
    uint64_t offset = ((uint64_t)virtAddr) % PAGE_SIZE;
    virtAddr = (void*)ROUND_DOWN(virtAddr, PAGE_SIZE);

    pml_t* level3 = pml_get(table, PML_GET_INDEX(virtAddr, 4));
    if (level3 == NULL)
    {
        return NULL;
    }

    pml_t* level2 = pml_get(level3, PML_GET_INDEX(virtAddr, 3));
    if (level2 == NULL)
    {
        return NULL;
    }

    pml_t* level1 = pml_get(level2, PML_GET_INDEX(virtAddr, 2));
    if (level1 == NULL)
    {
        return NULL;
    }

    pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];
    if (!(*entry & PAGE_PRESENT))
    {
        return NULL;
    }

    return (void*)(((uint64_t)PAGE_ENTRY_GET_ADDRESS(*entry)) + offset);
}

static inline bool pml_is_mapped(pml_t* table, const void* virtAddr, uint64_t pageAmount)
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

static inline bool pml_is_unmapped(pml_t* table, const void* virtAddr, uint64_t pageAmount)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        pml_t* level3 = pml_get(table, PML_GET_INDEX(virtAddr, 4));
        if (level3 == NULL)
        {
            virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
            continue;
        }

        pml_t* level2 = pml_get(level3, PML_GET_INDEX(virtAddr, 3));
        if (level2 == NULL)
        {
            virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
            continue;
        }

        pml_t* level1 = pml_get(level2, PML_GET_INDEX(virtAddr, 2));
        if (level1 == NULL)
        {
            virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
            continue;
        }

        pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];
        if (*entry & PAGE_PRESENT)
        {
            return false;
        }

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
    }

    return true;
}

static inline uint64_t pml_map(pml_t* table, void* virtAddr, void* physAddr, uint64_t pageAmount, uint64_t flags)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        pml_t* level3 =
            pml_get_or_alloc(table, PML_GET_INDEX(virtAddr, 4), (flags | PAGE_WRITE | PAGE_USER) & ~PAGE_GLOBAL);
        if (level3 == NULL)
        {
            return ERR;
        }

        pml_t* level2 = pml_get_or_alloc(level3, PML_GET_INDEX(virtAddr, 3), flags | PAGE_WRITE | PAGE_USER);
        if (level2 == NULL)
        {
            return ERR;
        }

        pml_t* level1 = pml_get_or_alloc(level2, PML_GET_INDEX(virtAddr, 2), flags | PAGE_WRITE | PAGE_USER);
        if (level1 == NULL)
        {
            return ERR;
        }

        pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];
        *entry = page_entry_create(physAddr, flags);

        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
        physAddr = (void*)((uint64_t)physAddr + PAGE_SIZE);
    }

    return 0;
}

static inline void pml_unmap(pml_t* table, void* virtAddr, uint64_t pageAmount)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        pml_t* level3 = pml_get(table, PML_GET_INDEX(virtAddr, 4));
        if (level3 == NULL)
        {
            continue;
        }

        pml_t* level2 = pml_get(level3, PML_GET_INDEX(virtAddr, 3));
        if (level2 == NULL)
        {
            continue;
        }

        pml_t* level1 = pml_get(level2, PML_GET_INDEX(virtAddr, 2));
        if (level1 == NULL)
        {
            continue;
        }

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

static inline uint64_t pml_get_entry(pml_t* table, void* virtAddr)
{
    pml_t* level3 = pml_get(table, PML_GET_INDEX(virtAddr, 4));
    if (level3 == NULL)
    {
        return ERR;
    }

    pml_t* level2 = pml_get(level3, PML_GET_INDEX(virtAddr, 3));
    if (level2 == NULL)
    {
        return ERR;
    }

    pml_t* level1 = pml_get(level2, PML_GET_INDEX(virtAddr, 2));
    if (level1 == NULL)
    {
        return ERR;
    }

    pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];
    if (!(*entry & PAGE_PRESENT))
    {
        return ERR;
    }

    return *entry;
}

static inline uint64_t pml_set_flags(pml_t* table, void* virtAddr, uint64_t pageAmount, uint64_t flags)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        pml_t* level3 = pml_get(table, PML_GET_INDEX(virtAddr, 4));
        if (level3 == NULL)
        {
            return ERR;
        }

        pml_t* level2 = pml_get(level3, PML_GET_INDEX(virtAddr, 3));
        if (level2 == NULL)
        {
            return ERR;
        }

        pml_t* level1 = pml_get(level2, PML_GET_INDEX(virtAddr, 2));
        if (level1 == NULL)
        {
            return ERR;
        }

        pml_entry_t* entry = &level1->entries[PML_GET_INDEX(virtAddr, 1)];
        if (!(*entry & PAGE_PRESENT))
        {
            return ERR;
        }

        uint64_t finalFlags = flags;
        if (*entry & PAGE_OWNED)
        {
            finalFlags |= PAGE_OWNED;
        }

        *entry = page_entry_create(VMM_HIGHER_TO_LOWER(PAGE_ENTRY_GET_ADDRESS(*entry)), finalFlags);

        PAGE_INVALIDATE(virtAddr);
        virtAddr = (void*)((uint64_t)virtAddr + PAGE_SIZE);
    }

    return 0;
}
