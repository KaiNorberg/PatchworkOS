#pragma once

#include "paging_types.h"

#include <assert.h>
#include <errno.h>
#include <sys/math.h>

static inline void page_invalidate(void* virtAddr)
{
    asm volatile("invlpg (%0)" : : "r"(virtAddr) : "memory");
}

static inline pml_entry_t pml_entry_create(void* physAddr, pml_flags_t flags, pml_callback_id_t callbackId)
{
    pml_entry_t entry = {0};
    entry.address = ((uintptr_t)physAddr >> 12);
    entry.present = 1;
    entry.write = (flags & PML_WRITE) ? 1 : 0;
    entry.user = (flags & PML_USER) ? 1 : 0;
    entry.writeThrough = (flags & PML_WRITE_THROUGH) ? 1 : 0;
    entry.cacheDisabled = (flags & PML_CACHE_DISABLED) ? 1 : 0;
    entry.accessed = (flags & PML_ACCESSED) ? 1 : 0;
    entry.dirty = (flags & PML_DIRTY) ? 1 : 0;
    entry.pageSize = (flags & PML_SIZE) ? 1 : 0;
    entry.global = (flags & PML_GLOBAL) ? 1 : 0;
    entry.owned = (flags & PML_OWNED) ? 1 : 0;
    entry.callbackId = callbackId;
    return entry;
}

static inline pml_t* pml_new(pml_alloc_page_t allocPage)
{
    pml_t* pml = (pml_t*)allocPage();
    if (pml == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    memset(pml, 0, PAGE_SIZE);
    return pml;
}

static inline void pml_free(pml_t* pml, int64_t level, pml_free_page_t freePage)
{
    if (level < 0)
    {
        return;
    }

    for (uint64_t i = 0; i < PML_ENTRY_AMOUNT; i++)
    {
        pml_entry_t entry = pml->entries[i];
        if (!entry.present)
        {
            continue;
        }

        if (level != 1 || entry.owned)
        {
            pml_free(PML_LOWER_TO_HIGHER((uint64_t)(entry.address << 12)), level - 1, freePage);
        }
    }

    freePage(pml);
}

static inline uint64_t page_table_init(page_table_t* table, pml_alloc_page_t allocPage, pml_free_page_t freePage)
{
    table->pml4 = pml_new(allocPage);
    if (table->pml4 == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }
    table->allocPage = allocPage;
    table->freePage = freePage;
    return 0;
}

static inline void page_table_deinit(page_table_t* table)
{
    pml_free(table->pml4, 4, table->freePage);
}

static inline void page_table_load(page_table_t* table)
{
    uint64_t cr3 = (uint64_t)PML_HIGHER_TO_LOWER(table->pml4);
    if (cr3_read() != cr3)
    {
        cr3_write(cr3);
    }
}

static inline pml_t* page_table_get_pml(page_table_t* table, pml_t* level, uint64_t index, pml_flags_t flags,
    pml_callback_id_t callbackId, bool shouldAllocate)
{
    pml_entry_t entry = level->entries[index];
    if (entry.present)
    {
        return PML_LOWER_TO_HIGHER((uint64_t)(entry.address << 12));
    }
    else if (shouldAllocate)
    {
        pml_t* pml = pml_new(table->allocPage);
        if (pml == NULL)
        {
            return NULL;
        }
        level->entries[index] = pml_entry_create(PML_HIGHER_TO_LOWER(pml), flags, callbackId);
        return pml;
    }

    return NULL;
}

typedef struct
{
    pml_t* pml3;
    pml_t* pml2;
    pml_t* pml1;
    uint64_t oldIdx3;
    uint64_t oldIdx2;
    uint64_t oldIdx1;
} page_table_traverse_t;

/**
 * @brief Allows for fast traversal of the page table by caching previously accessed layers.
 *
 * @param table The page table.
 * @param traverse The helper structure used to cache each layer.
 * @param virtAddr The target virtual address.
 * @param shouldAllocate If missing levels should be allocated during traversal.
 * @param flags The flags that should be assigned to newly allocated levels, ignored if `shouldAllocate` is `false`.
 * @return `true` if a pml1 exists for the current address or was successfully allocated, `false` otherwise.
 */
static inline bool page_table_traverse(page_table_t* table, page_table_traverse_t* traverse, const void* virtAddr,
    bool shouldAllocate, pml_flags_t flags)
{
    // Lazy initialize traversal.
    if (traverse->pml3 == NULL || traverse->pml2 == NULL || traverse->pml1 == NULL)
    {
        traverse->oldIdx3 = PML_GET_INDEX(virtAddr, 4);
        traverse->pml3 = page_table_get_pml(table, table->pml4, traverse->oldIdx3,
            (flags | PML_WRITE | PML_USER) & ~PML_GLOBAL, PML_CALLBACK_NONE, shouldAllocate);
        if (traverse->pml3 == NULL)
        {
            return false;
        }

        traverse->oldIdx2 = PML_GET_INDEX(virtAddr, 3);
        traverse->pml2 = page_table_get_pml(table, traverse->pml3, traverse->oldIdx2, flags | PML_WRITE | PML_USER,
            PML_CALLBACK_NONE, shouldAllocate);
        if (traverse->pml2 == NULL)
        {
            return false;
        }

        traverse->oldIdx1 = PML_GET_INDEX(virtAddr, 2);
        traverse->pml1 = page_table_get_pml(table, traverse->pml2, traverse->oldIdx1, flags | PML_WRITE | PML_USER,
            PML_CALLBACK_NONE, shouldAllocate);
        if (traverse->pml1 == NULL)
        {
            return false;
        }

        return true;
    }

    uint64_t newIdx1 = PML_GET_INDEX(virtAddr, 2);
    if (traverse->oldIdx1 == newIdx1)
    {
        return true;
    }

    traverse->oldIdx1 = newIdx1;

    uint64_t newIdx2 = PML_GET_INDEX(virtAddr, 3);

    if (traverse->oldIdx2 == newIdx2)
    {
        traverse->pml1 = page_table_get_pml(table, traverse->pml2, newIdx1, flags | PML_WRITE | PML_USER,
            PML_CALLBACK_NONE, shouldAllocate);
        return traverse->pml1 != NULL;
    }

    traverse->oldIdx2 = newIdx2;

    uint64_t newIdx3 = PML_GET_INDEX(virtAddr, 4);

    if (traverse->oldIdx3 == newIdx3)
    {
        traverse->pml2 = page_table_get_pml(table, traverse->pml3, newIdx2, flags | PML_WRITE | PML_USER,
            PML_CALLBACK_NONE, shouldAllocate);
        if (traverse->pml2 == NULL)
        {
            return false;
        }

        traverse->pml1 = page_table_get_pml(table, traverse->pml2, newIdx1, flags | PML_WRITE | PML_USER,
            PML_CALLBACK_NONE, shouldAllocate);
        return traverse->pml1 != NULL;
    }

    traverse->oldIdx3 = newIdx3;

    traverse->pml3 = page_table_get_pml(table, table->pml4, newIdx3, (flags | PML_WRITE | PML_USER) & ~PML_GLOBAL,
        PML_CALLBACK_NONE, shouldAllocate);
    if (traverse->pml3 == NULL)
    {
        return false;
    }

    traverse->pml2 = page_table_get_pml(table, traverse->pml3, newIdx2, flags | PML_WRITE | PML_USER, PML_CALLBACK_NONE,
        shouldAllocate);
    if (traverse->pml2 == NULL)
    {
        return false;
    }

    traverse->pml1 = page_table_get_pml(table, traverse->pml2, newIdx1, flags | PML_WRITE | PML_USER, PML_CALLBACK_NONE,
        shouldAllocate);
    return traverse->pml1 != NULL;
}

static inline void* page_table_get_phys_addr(page_table_t* table, const void* virtAddr)
{
    uint64_t offset = ((uint64_t)virtAddr) % PAGE_SIZE;
    virtAddr = (void*)ROUND_DOWN(virtAddr, PAGE_SIZE);

    page_table_traverse_t traverse = {0};
    if (!page_table_traverse(table, &traverse, virtAddr, false, PML_NONE))
    {
        return NULL;
    }

    pml_entry_t entry = traverse.pml1->entries[PML_GET_INDEX(virtAddr, 1)];
    if (!entry.present)
    {
        return NULL;
    }

    return (void*)((entry.address << 12) + offset);
}

static inline bool page_table_is_mapped(page_table_t* table, const void* virtAddr, uint64_t pageAmount)
{
    page_table_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, virtAddr, false, PML_NONE))
        {
            return false;
        }

        pml_entry_t entry = traverse.pml1->entries[PML_GET_INDEX(virtAddr, 1)];
        if (!entry.present)
        {
            return false;
        }

        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }

    return true;
}

static inline bool page_table_is_unmapped(page_table_t* table, const void* virtAddr, uint64_t pageAmount)
{
    page_table_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, virtAddr, false, PML_NONE))
        {
            continue;
        }

        pml_entry_t entry = traverse.pml1->entries[PML_GET_INDEX(virtAddr, 1)];
        if (entry.present)
        {
            return false;
        }

        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }

    return true;
}

static inline uint64_t page_table_map(page_table_t* table, void* virtAddr, void* physAddr, uint64_t pageAmount,
    pml_flags_t flags, pml_callback_id_t callbackId)
{
    page_table_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, virtAddr, true, flags))
        {
            errno = ENOMEM;
            return ERR;
        }

        uint64_t idx0 = PML_GET_INDEX((uintptr_t)virtAddr, 1);
        assert(!traverse.pml1->entries[idx0].present); // If this happens the vmm is broken.
        traverse.pml1->entries[idx0] = pml_entry_create(physAddr, flags, callbackId);

        physAddr = (void*)((uint64_t)physAddr + PAGE_SIZE);
        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }

    return 0;
}

static inline void page_table_unmap(page_table_t* table, void* virtAddr, uint64_t pageAmount)
{
    page_table_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, virtAddr, false, 0))
        {
            continue;
        }
        uint64_t idx0 = PML_GET_INDEX(virtAddr, 1);

        pml_entry_t entry = traverse.pml1->entries[idx0];
        if (entry.owned)
        {
            pmm_free(PML_LOWER_TO_HIGHER((uint64_t)(entry.address << 12)));
        }

        traverse.pml1->entries[idx0].raw = 0;
        page_invalidate(virtAddr);

        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }
}

static inline void page_table_collect_callbacks(page_table_t* table, void* virtAddr, uint64_t pageAmount,
    uint64_t* callbacks)
{
    page_table_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, virtAddr, false, 0))
        {
            continue;
        }
        uint64_t idx0 = PML_GET_INDEX(virtAddr, 1);

        pml_entry_t entry = traverse.pml1->entries[idx0];
        if (entry.present && entry.callbackId != PML_CALLBACK_NONE)
        {
            callbacks[entry.callbackId]++;
        }

        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }
}

static inline uint64_t page_table_set_flags(page_table_t* table, void* virtAddr, uint64_t pageAmount, pml_flags_t flags)
{
    page_table_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, virtAddr, false, 0))
        {
            continue;
        }
        uint64_t idx0 = PML_GET_INDEX(virtAddr, 1);

        pml_entry_t entry = traverse.pml1->entries[idx0];
        if (!entry.present)
        {
            errno = ENOENT;
            return ERR;
        }

        uint64_t finalFlags = flags;
        if (traverse.pml1->entries[idx0].owned)
        {
            finalFlags |= PML_OWNED;
        }

        traverse.pml1->entries[idx0] = pml_entry_create((void*)((uintptr_t)traverse.pml1->entries[idx0].address << 12),
            finalFlags, traverse.pml1->entries[idx0].callbackId);

        page_invalidate(virtAddr);

        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }

    return 0;
}

static inline void* page_table_find_first_mapped_page(page_table_t* table, void* startAddr, void* endAddr)
{
    void* currentAddr = (void*)ROUND_DOWN((uintptr_t)startAddr, PAGE_SIZE);

    page_table_traverse_t traverse = {0};

    while ((uintptr_t)currentAddr < (uintptr_t)endAddr)
    {
        if (page_table_traverse(table, &traverse, currentAddr, false, 0))
        {
            uint64_t pml1Idx = PML_GET_INDEX(currentAddr, 1);
            if (traverse.pml1->entries[pml1Idx].present)
            {
                return currentAddr;
            }
        }

        currentAddr = (void*)((uintptr_t)currentAddr + PAGE_SIZE);
    }

    return NULL;
}
