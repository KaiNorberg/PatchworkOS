#pragma once

#include "cpu/regs.h"
#include "defs.h"
#include "mem/pmm.h"

#include <assert.h>
#include <sys/math.h>

/**
 * @brief Page Tables.
 * @defgroup kernel_mem_pml PML
 * @ingroup kernel_mem
 *
 * The `pml.h` file stores functions for manipulating page tables.
 *
 */

typedef enum
{
    PML_PRESENT = (1 << 0),
    PML_WRITE = (1 << 1),
    PML_USER = (1 << 2),
    PML_WRITE_THROUGH = (1 << 3),
    PML_CACHE_DISABLED = (1 << 4),
    PML_ACCESSED = (1 << 5),
    PML_DIRTY = (1 << 6),
    PML_SIZE = (1 << 7),
    PML_GLOBAL = (1 << 8),
    PML_OWNED = (1 << 9)
} pml_flags_t;

#define PML_ENTRY_AMOUNT 512

#define PML_HIGHER_HALF_END (UINTPTR_MAX - 0xFFF)
#define PML_HIGHER_HALF_START 0xFFFF800000000000
#define PML_LOWER_HALF_END (0x7FFFFFFFF000)
#define PML_LOWER_HALF_START 0x400000

#define PML_HIGHER_TO_LOWER(address) ((void*)((uint64_t)(address) - PML_HIGHER_HALF_START))
#define PML_LOWER_TO_HIGHER(address) ((void*)((uint64_t)(address) + PML_HIGHER_HALF_START))

// Allows a address to be specified in either the upper or lower half
#define PML_ENSURE_LOWER_HALF(address) \
    (address >= (void*)PML_HIGHER_HALF_START ? PML_HIGHER_TO_LOWER(address) : address)

#define PML_GET_INDEX(address, level) (((uint64_t)(address) >> (((level) - 1) * 9 + 12)) & 0x1FF)

#define PML_MAX_CALLBACK (1 << 7)
#define PML_CALLBACK_NONE (1 << 7)

typedef uint8_t pml_callback_id_t;

typedef union {
    uint64_t raw;
    struct
    {
        uint64_t present : 1;
        uint64_t write : 1;
        uint64_t user : 1;
        uint64_t writeThrough : 1;
        uint64_t cacheDisabled : 1;
        uint64_t accessed : 1;
        uint64_t dirty : 1;
        uint64_t pageSize : 1;
        uint64_t global : 1;
        /**
         * @brief Defined by Patchwork. Specifies if the page is owned by the page pml4 and should be freed when the
         * page is unmapped.
         */
        uint64_t owned : 1;
        uint64_t available1 : 2;
        uint64_t address : 40;
        /**
         * @brief Defined by Patchwork. Specifies the id of the associated callback of the page.
         */
        uint64_t callbackId : 7;
        uint64_t protection : 4;
        uint64_t noExecute : 1;
    };
} pml_entry_t;

typedef struct pml
{
    pml_entry_t entries[PML_ENTRY_AMOUNT];
} pml_t;

static inline void pml_page_invalidate(void* virtAddr)
{
    asm volatile("invlpg (%0)" : : "r"(virtAddr) : "memory");
}

static pml_entry_t pml_entry_create(void* physAddr, pml_flags_t flags, pml_callback_id_t callbackId)
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

static pml_t* pml_get(pml_t* pml, uint64_t index, pml_flags_t flags, pml_callback_id_t callbackId)
{
    pml_entry_t entry = pml->entries[index];
    if (!entry.present)
    {
        return NULL;
    }
    return PML_LOWER_TO_HIGHER((uint64_t)(entry.address << 12));
}

/**
 * @brief Retrieves an entry from the pml or allocates it if it does not exist.
 *
 * @param pml The source pml.
 * @param index The index in the pml.
 * @param flags Ignored so that pml_traversal can perform a safe function pointer case.
 * @param callbackId Ignored so that pml_traversal can perform a safe function pointer case.
 * @return The found or allocated pml.
 */
static pml_t* pml_get_or_alloc(pml_t* pml, uint64_t index, pml_flags_t flags __attribute__((unused)),
    pml_callback_id_t callbackId __attribute__((unused)))
{
    pml_entry_t entry = pml->entries[index];
    if (entry.present)
    {
        return PML_LOWER_TO_HIGHER((uint64_t)(entry.address << 12));
    }
    else
    {
        pml_t* address = pmm_alloc();
        if (address == NULL)
        {
            return NULL;
        }
        memset(address, 0, PAGE_SIZE);
        pml->entries[index] = pml_entry_create(PML_HIGHER_TO_LOWER(address), flags, callbackId);
        return address;
    }
}

typedef struct
{
    pml_t* pml3;
    pml_t* pml2;
    pml_t* pml1;
    uint64_t oldIdx3;
    uint64_t oldIdx2;
    uint64_t oldIdx1;
} pml_traverse_t;

/**
 * @brief Allows for fast traversal of the page table by caching previously accessed layers.
 *
 * @param pml4 The page table.
 * @param traverse The helper structure used to cache each layer.
 * @param virtAddr The target virtual address.
 * @param shouldAllocate If missing levels should be allocated during traversal.
 * @param flags The flags that should be assigned to newly allocated levels, ignored if `shouldAllocate` is `false`.
 * `false`.
 * @return `true` if a pml1 exists for the current address or was successfully allocated, `false` otherwise.
 */
static inline bool pml_traverse(pml_t* pml4, pml_traverse_t* traverse, const void* virtAddr, bool shouldAllocate,
    pml_flags_t flags)
{
    pml_t* (*get_func)(pml_t*, uint64_t, pml_flags_t, pml_callback_id_t) = shouldAllocate ? pml_get_or_alloc : pml_get;

    // Lazy initialize traversal.
    if (traverse->pml3 == NULL || traverse->pml2 == NULL || traverse->pml1 == NULL)
    {
        traverse->oldIdx3 = PML_GET_INDEX(virtAddr, 4);
        traverse->pml3 =
            get_func(pml4, traverse->oldIdx3, (flags | PML_WRITE | PML_USER) & ~PML_GLOBAL, PML_CALLBACK_NONE);
        if (traverse->pml3 == NULL)
        {
            return false;
        }

        traverse->oldIdx2 = PML_GET_INDEX(virtAddr, 3);
        traverse->pml2 = get_func(traverse->pml3, traverse->oldIdx2, flags | PML_WRITE | PML_USER, PML_CALLBACK_NONE);
        if (traverse->pml2 == NULL)
        {
            return false;
        }

        traverse->oldIdx1 = PML_GET_INDEX(virtAddr, 2);
        traverse->pml1 = get_func(traverse->pml2, traverse->oldIdx1, flags | PML_WRITE | PML_USER, PML_CALLBACK_NONE);
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
        traverse->pml1 = get_func(traverse->pml2, newIdx1, flags | PML_WRITE | PML_USER, PML_CALLBACK_NONE);
        return traverse->pml1 != NULL;
    }

    traverse->oldIdx2 = newIdx2;

    uint64_t newIdx3 = PML_GET_INDEX(virtAddr, 4);

    if (traverse->oldIdx3 == newIdx3)
    {
        traverse->pml2 = get_func(traverse->pml3, newIdx2, flags | PML_WRITE | PML_USER, PML_CALLBACK_NONE);
        if (traverse->pml2 == NULL)
        {
            return false;
        }

        traverse->pml1 = get_func(traverse->pml2, newIdx1, flags | PML_WRITE | PML_USER, PML_CALLBACK_NONE);
        return traverse->pml1 != NULL;
    }

    traverse->oldIdx3 = newIdx3;

    traverse->pml3 = get_func(pml4, newIdx3, (flags | PML_WRITE | PML_USER) & ~PML_GLOBAL, PML_CALLBACK_NONE);
    if (traverse->pml3 == NULL)
    {
        return false;
    }

    traverse->pml2 = get_func(traverse->pml3, newIdx2, flags | PML_WRITE | PML_USER, PML_CALLBACK_NONE);
    if (traverse->pml2 == NULL)
    {
        return false;
    }

    traverse->pml1 = get_func(traverse->pml2, newIdx1, flags | PML_WRITE | PML_USER, PML_CALLBACK_NONE);
    return traverse->pml1 != NULL;
}

static void pml_free_level(pml_t* pml4, int64_t level)
{
    if (level < 0)
    {
        return;
    }

    for (uint64_t i = 0; i < PML_ENTRY_AMOUNT; i++)
    {
        pml_entry_t entry = pml4->entries[i];
        if (!entry.present)
        {
            continue;
        }

        if (level != 1 || entry.owned)
        {
            pml_free_level(PML_LOWER_TO_HIGHER((uint64_t)(entry.address << 12)), level - 1);
        }
    }

    pmm_free(pml4);
}

static inline pml_t* pml_new(void)
{
    pml_t* pml4 = pmm_alloc();
    if (pml4 == NULL)
    {
        return NULL;
    }

    memset(pml4, 0, PAGE_SIZE);
    return pml4;
}

static inline void pml_free(pml_t* pml4)
{
    pml_free_level(pml4, 4);
}

static inline void pml_load(pml_t* pml4)
{
    uint64_t cr3 = (uint64_t)PML_HIGHER_TO_LOWER(pml4);
    if (cr3_read() != cr3)
    {
        cr3_write(cr3);
    }
}

static inline void* pml_phys_addr(pml_t* pml4, const void* virtAddr)
{
    uint64_t offset = ((uint64_t)virtAddr) % PAGE_SIZE;
    virtAddr = (void*)ROUND_DOWN(virtAddr, PAGE_SIZE);

    pml_traverse_t traverse = {0};
    if (!pml_traverse(pml4, &traverse, virtAddr, false, 0))
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

static inline bool pml_is_mapped(pml_t* pml4, const void* virtAddr, uint64_t pageAmount)
{
    pml_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!pml_traverse(pml4, &traverse, virtAddr, false, 0))
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

static inline bool pml_is_unmapped(pml_t* pml4, const void* virtAddr, uint64_t pageAmount)
{
    pml_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!pml_traverse(pml4, &traverse, virtAddr, false, 0))
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

static inline uint64_t pml_map(pml_t* pml4, void* virtAddr, void* physAddr, uint64_t pageAmount, pml_flags_t flags,
    pml_callback_id_t callbackId)
{
    pml_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!pml_traverse(pml4, &traverse, virtAddr, true, flags))
        {
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

static inline void pml_collect_callbacks(pml_t* pml4, void* virtAddr, uint64_t pageAmount, uint64_t* callbacks)
{
    pml_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!pml_traverse(pml4, &traverse, virtAddr, false, 0))
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

static inline void pml_unmap(pml_t* pml4, void* virtAddr, uint64_t pageAmount)
{
    pml_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!pml_traverse(pml4, &traverse, virtAddr, false, 0))
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
        pml_page_invalidate(virtAddr);

        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }
}

static inline uint64_t pml_set_flags(pml_t* pml4, void* virtAddr, uint64_t pageAmount, pml_flags_t flags)
{
    pml_traverse_t traverse = {0};

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!pml_traverse(pml4, &traverse, virtAddr, false, 0))
        {
            continue;
        }
        uint64_t idx0 = PML_GET_INDEX(virtAddr, 1);

        pml_entry_t entry = traverse.pml1->entries[idx0];
        if (!entry.present)
        {
            return ERR;
        }

        uint64_t finalFlags = flags;
        if (traverse.pml1->entries[idx0].owned)
        {
            finalFlags |= PML_OWNED;
        }

        traverse.pml1->entries[idx0] = pml_entry_create((void*)((uintptr_t)traverse.pml1->entries[idx0].address << 12),
            finalFlags, traverse.pml1->entries[idx0].callbackId);

        pml_page_invalidate(virtAddr);

        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }

    return 0;
}

static inline void* pml_find_first_mapped_page(pml_t* pml4, void* startAddr, void* endAddr)
{
    void* currentAddr = (void*)ROUND_DOWN((uintptr_t)startAddr, PAGE_SIZE);

    pml_traverse_t traverse = {0};

    while ((uintptr_t)currentAddr < (uintptr_t)endAddr)
    {
        if (pml_traverse(pml4, &traverse, currentAddr, false, 0))
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