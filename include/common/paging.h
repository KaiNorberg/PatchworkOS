#pragma once

#include "paging_types.h"
#include "regs.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/proc.h>

/**
 * @addtogroup common_paging
 * @{
 */

/**
 * @brief Invalidates a region of pages in the TLB.
 *
 * Even if a page table entry is modified, the CPU might still use a cached version of the entry in the TLB. To ensure
 * our changes are detected we must invalidate this cache using `invlpg` or if many pages are changed, a full TLB flush
 * by reloading CR3.
 *
 * @param virtAddr The virtual address of the page to invalidate.
 */
static inline void pml_invalidate_pages(page_table_t* table, uintptr_t virtAddr, uint64_t pageCount)
{
    if (pageCount == 0)
    {
        return;
    }

    uint64_t cr3 = cr3_read();
    if (cr3 != PML_ENSURE_LOWER_HALF(table->pml4))
    {
        // If the page table is not loaded we dont need to invalidate anything as the TLB is for the currently loaded
        // page table.
        return;
    }

    if (pageCount > 16)
    {
        cr3_write(cr3);
    }
    else
    {
        for (uint64_t i = 0; i < pageCount; i++)
        {
            asm volatile("invlpg (%0)" ::"r"(virtAddr + i * PAGE_SIZE) : "memory");
        }
    }
}

/**
 * @brief Retrieves the address from a page table entry and converts it to an accessible address.
 *
 * The accessible address depends on if we are in the kernel or the bootloader as the bootloader has physical memory
 * identity mapped to the higher half of the address space, while the kernel does not and instead has the higher half
 * mapped to the lower half of the address space.
 *
 * @param entry The page table entry.
 * @return The accessible address contained in the entry.
 */
static inline uintptr_t pml_accessible_addr(pml_entry_t entry)
{
#ifdef __BOOT__
    return entry.addr << PML_ADDR_OFFSET_BITS;
#elif __KERNEL__
    return PML_LOWER_TO_HIGHER(entry.addr << PML_ADDR_OFFSET_BITS);
#else
#error
#endif
}

/**
 * @brief Allocates and initializes a new page table level.
 *
 * @param table The page table.
 * @param outPml Will be filled with the newly allocated page table level.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline uint64_t pml_new(page_table_t* table, pml_t** outPml)
{
    pml_t* pml;
    if (table->allocPages((void**)&pml, 1) == ERR)
    {
        return ERR;
    }
    memset(pml, 0, PAGE_SIZE);
    *outPml = pml;
    return 0;
}

/**
 * @brief Recursively frees a page table level, all its children and any owned pages.
 *
 * @param table The page table.
 * @param pml The current page table level to free.
 * @param level The current level of the page table.
 */
static inline void pml_free(page_table_t* table, pml_t* pml, pml_level_t level)
{
    if (level < 0)
    {
        return;
    }

    for (pml_index_t i = 0; i < PML_INDEX_AMOUNT; i++)
    {
        pml_entry_t* entry = &pml->entries[i];
        if (!entry->present)
        {
            continue;
        }

        if (level > PML1)
        {
            pml_free(table, (pml_t*)pml_accessible_addr(*entry), level - 1);
        }
        else if (entry->owned)
        {
            void* addr = (void*)pml_accessible_addr(*entry);
            table->freePages(&addr, 1);
        }
    }

    table->freePages((void**)&pml, 1);
}

/**
 * @brief Initializes a page table.
 *
 * @param table The page table to initialize.
 * @param allocPages The function to use for allocating pages.
 * @param freePages The function to use for freeing pages.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline uint64_t page_table_init(page_table_t* table, pml_alloc_pages_t allocPages, pml_free_pages_t freePages)
{
    table->allocPages = allocPages;
    table->freePages = freePages;
    if (pml_new(table, &table->pml4) == ERR)
    {
        return ERR;
    }
    return 0;
}

/**
 * @brief Deinitializes a page table, freeing all allocated pages.
 *
 * @param table The page table to deinitialize.
 */
static inline void page_table_deinit(page_table_t* table)
{
    pml_free(table, table->pml4, PML4);
}

/**
 * @brief Loads the page table into the CR3 register if it is not already loaded.
 *
 * @param table The page table to load.
 */
static inline void page_table_load(page_table_t* table)
{
    uint64_t cr3 = PML_ENSURE_LOWER_HALF(table->pml4);
    if (cr3 != cr3_read())
    {
        cr3_write(cr3);
    }
}

/**
 * @brief Retrieves or allocates the next level page table.
 *
 * If the entry at the specified index is present, it retrieves the corresponding page table level.
 * If the entry is not present and the `PML_PRESENT` flag is set in `flags`, it allocates a new page table level, and
 * initializes it with the provided flags and callback ID. If the entry is not present and the `PML_PRESENT` flag is not
 * set, it returns `ERR`.
 *
 * @param table The page table.
 * @param currentPml The current page table level.
 * @param index The index within the current page table level.
 * @param flags The flags to assign to a newly allocated page table level, if applicable.
 * @param callbackId The callback ID to assign to a newly allocated page table level, if applicable.
 * @param outPml Will be filled with the retrieved or newly allocated page table level.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline uint64_t page_table_get_pml(page_table_t* table, pml_t* currentPml, pml_index_t index, pml_flags_t flags,
    pml_t** outPml)
{
    pml_entry_t* entry = &currentPml->entries[index];
    if (entry->present)
    {
        *outPml = (pml_t*)pml_accessible_addr(*entry);
        return 0;
    }
    else if (flags & PML_PRESENT)
    {
        pml_t* nextPml;
        if (pml_new(table, &nextPml) == ERR)
        {
            return ERR;
        }
        currentPml->entries[index].raw = (flags & PML_FLAGS_MASK) | (PML_ENSURE_LOWER_HALF(nextPml) & PML_ADDR_MASK);
        *outPml = nextPml;
        return 0;
    }

    return ERR;
}

/**
 * @brief Helper structure for fast traversal of the page table.
 * @struct page_table_traverse_t
 */
typedef struct
{
    pml_t* pml3;
    pml_t* pml2;
    pml_t* pml1;
    bool pml3Valid;
    bool pml2Valid;
    bool pml1Valid;
    pml_index_t oldIdx3;
    pml_index_t oldIdx2;
    pml_index_t oldIdx1;
    pml_entry_t* entry;
} page_table_traverse_t;

static inline void page_table_traverse_init(page_table_traverse_t* traverse)
{
    traverse->pml3Valid = false;
    traverse->pml2Valid = false;
    traverse->pml1Valid = false;
    traverse->oldIdx3 = PML_INDEX_INVALID;
    traverse->oldIdx2 = PML_INDEX_INVALID;
    traverse->oldIdx1 = PML_INDEX_INVALID;
}

/**
 * @brief Allows for fast traversal of the page table by caching previously accessed layers.
 *
 * If the present flag is not set in `flags` then no new levels will be allocated and if non present pages are
 * encountered the function will return `false`.
 *
 * Note that higher level flags are or'd with `PML_WRITE | PML_USER` since only the permissions of a higher level will
 * apply to lower levels, meaning that the lowest level should be the one with the actual desired permissions.
 *
 * @param table The page table.
 * @param traverse The helper structure used to cache each layer.
 * @param virtAddr The target virtual address.
 * @param flags The flags to assigned to newly allocated levels, if the present flag is not set then dont allocate new
 * levels.
 * @return `true` if a pml1 exists for the current address or was successfully allocated, `false` otherwise.
 */
static inline bool page_table_traverse(page_table_t* table, page_table_traverse_t* traverse, uintptr_t virtAddr,
    pml_flags_t flags)
{
    pml_index_t newIdx3 = PML_ADDR_TO_INDEX(virtAddr, PML4);
    if (!traverse->pml3Valid || traverse->oldIdx3 != newIdx3)
    {
        if (page_table_get_pml(table, table->pml4, newIdx3, (flags | PML_WRITE | PML_USER) & ~PML_GLOBAL,
                &traverse->pml3) == ERR)
        {
            return false;
        }
        traverse->oldIdx3 = newIdx3;
        traverse->pml2Valid = false; // Invalidate cache for lower levels
    }

    pml_index_t newIdx2 = PML_ADDR_TO_INDEX(virtAddr, PML3);
    if (!traverse->pml2Valid || traverse->oldIdx2 != newIdx2)
    {
        if (page_table_get_pml(table, traverse->pml3, newIdx2, flags | PML_WRITE | PML_USER, &traverse->pml2) == ERR)
        {
            return false;
        }
        traverse->oldIdx2 = newIdx2;
        traverse->pml1Valid = false; // Invalidate cache for lower levels
    }

    pml_index_t newIdx1 = PML_ADDR_TO_INDEX(virtAddr, PML2);
    if (!traverse->pml1Valid || traverse->oldIdx1 != newIdx1)
    {
        if (page_table_get_pml(table, traverse->pml2, newIdx1, flags | PML_WRITE | PML_USER, &traverse->pml1) == ERR)
        {
            return false;
        }
        traverse->oldIdx1 = newIdx1;
    }

    traverse->entry = &traverse->pml1->entries[PML_ADDR_TO_INDEX(virtAddr, PML1)];
    return true;
}

/**
 * @brief Retrieves the physical address mapped to a given virtual address.
 *
 * If the virtual address is not mapped, the function returns `ERR`.
 *
 * @param table The page table.
 * @param virtAddr The virtual address to look up.
 * @param outPhysAddr Will be filled with the corresponding physical address on success.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline uint64_t page_table_get_phys_addr(page_table_t* table, const void* virtAddr, void** outPhysAddr)
{
    uint64_t offset = ((uint64_t)virtAddr) % PAGE_SIZE;
    virtAddr = (void*)ROUND_DOWN(virtAddr, PAGE_SIZE);

    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr, PML_NONE))
    {
        return ERR;
    }

    if (!traverse.entry->present)
    {
        return ERR;
    }

    *outPhysAddr = (void*)((traverse.entry->addr << PML_ADDR_OFFSET_BITS) + offset);
    return 0;
}

/**
 * @brief Checks if a range of virtual addresses is completely mapped.
 *
 * If any page in the range is not mapped, the function returns `false`.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pageAmount The number of pages to check.
 * @return `true` if the entire range is mapped, `false` otherwise.
 */
static inline bool page_table_is_mapped(page_table_t* table, const void* virtAddr, uint64_t pageAmount)
{
    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr + i * PAGE_SIZE, PML_NONE))
        {
            return false;
        }

        if (!traverse.entry->present)
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Checks if a range of virtual addresses is completely unmapped.
 *
 * If any page in the range is mapped, the function returns `false`.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pageAmount The number of pages to check.
 * @return `true` if the entire range is unmapped, `false` otherwise.
 */
static inline bool page_table_is_unmapped(page_table_t* table, const void* virtAddr, uint64_t pageAmount)
{
    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (traverse.entry->present)
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Maps a range of virtual addresses to physical addresses in the page table.
 *
 * If any page in the range is already mapped, the function will fail and return `ERR`.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param physAddr The starting physical address.
 * @param pageAmount The number of pages to map.
 * @param flags The flags to set for the mapped pages. Must include `PML_PRESENT`.
 * @param callbackId The callback ID to associate with the mapped pages or `PML_CALLBACK_NONE`.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline uint64_t page_table_map(page_table_t* table, void* virtAddr, void* physAddr, uint64_t pageAmount,
    pml_flags_t flags, pml_callback_id_t callbackId)
{
    if (!(flags & PML_PRESENT))
    {
        return ERR;
    }

    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr, flags))
        {
            return ERR;
        }

        if (traverse.entry->present)
        {
            return ERR;
        }

        traverse.entry->raw = flags;
        traverse.entry->addr = ((uintptr_t)PML_ENSURE_LOWER_HALF(physAddr)) >> PML_ADDR_OFFSET_BITS;
        traverse.entry->callbackId = callbackId;

        physAddr = (void*)((uintptr_t)physAddr + PAGE_SIZE);
        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }

    return 0;
}

/**
 * @brief Maps an array of physical pages to contiguous virtual addresses in the page table.
 *
 * If any page in the range is already mapped, the function will fail and return `ERR`.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pages Array of physical page addresses to map.
 * @param pageAmount The number of pages in the array to map.
 * @param flags The flags to set for the mapped pages. Must include `PML_PRESENT`.
 * @param callbackId The callback ID to associate with the mapped pages or `PML_CALLBACK_NONE`.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline uint64_t page_table_map_pages(page_table_t* table, void* virtAddr, void** pages, uint64_t pageAmount,
    pml_flags_t flags, pml_callback_id_t callbackId)
{
    if (!(flags & PML_PRESENT))
    {
        return ERR;
    }

    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr, flags))
        {
            return ERR;
        }

        if (traverse.entry->present)
        {
            return ERR;
        }

        traverse.entry->raw = flags;
        traverse.entry->addr = ((uintptr_t)PML_ENSURE_LOWER_HALF(pages[i])) >> PML_ADDR_OFFSET_BITS;
        traverse.entry->callbackId = callbackId;

        virtAddr = (void*)((uintptr_t)virtAddr + PAGE_SIZE);
    }

    return 0;
}

/**
 * @brief Unmaps a range of virtual addresses from the page table.
 *
 * If a page is not currently mapped, it is skipped.
 *
 * If the `PML_OWNED` flag is set on a page, the physical page will be freed.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pageAmount The number of pages to unmap.
 */
static inline void page_table_unmap(page_table_t* table, void* virtAddr, uint64_t pageAmount)
{
    void* pageBuffer[PML_PAGE_BUFFER_SIZE];
    uint64_t pageBufferLength = 0;

    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            continue;
        }

        if (traverse.entry->pinDepth != 0)
        {
            continue;
        }

        if (traverse.entry->owned)
        {
            pageBuffer[pageBufferLength++] = (void*)pml_accessible_addr(*traverse.entry);
            if (pageBufferLength >= PML_PAGE_BUFFER_SIZE)
            {
                table->freePages(pageBuffer, pageBufferLength);
                pageBufferLength = 0;
            }
        }

        traverse.entry->raw = 0;
    }

    if (pageBufferLength > 0)
    {
        table->freePages(pageBuffer, pageBufferLength);
    }

    pml_invalidate_pages(table, (uintptr_t)virtAddr, pageAmount);
}

/**
 * @brief Collects the number of pages associated with each callback ID in the specified range.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pageAmount The number of pages to check.
 * @param callbacks An array of size `PML_MAX_CALLBACK` that will be filled with the occurrences of each callback ID.
 */
static inline void page_table_collect_callbacks(page_table_t* table, void* virtAddr, uint64_t pageAmount,
    uint64_t* callbacks)
{
    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            continue;
        }

        if (traverse.entry->callbackId != PML_CALLBACK_NONE)
        {
            callbacks[traverse.entry->callbackId]++;
        }
    }
}

/**
 * @brief Sets the flags for a range of pages in the page table.
 *
 * If a page is not currently mapped, it is skipped.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pageAmount The number of pages to update.
 * @param flags The new flags to set. The `PML_OWNED` flag is preserved.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline uint64_t page_table_set_flags(page_table_t* table, void* virtAddr, uint64_t pageAmount, pml_flags_t flags)
{
    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            return ERR;
        }

        if (traverse.entry->pinDepth != 0)
        {
            return ERR;
        }

        if (traverse.entry->owned)
        {
            flags |= PML_OWNED;
        }

        // Bit magic to only update the flags while preserving the address and callback ID.
        traverse.entry->raw = (traverse.entry->raw & ~PML_FLAGS_MASK) | (flags & PML_FLAGS_MASK);
    }

    pml_invalidate_pages(table, (uintptr_t)virtAddr, pageAmount);
    return 0;
}

/**
 * @brief Finds the first contiguous unmapped region with the given number of pages within the specified address range.
 *
 * Good luck with this function, im like 99% sure it works.
 *
 * @param table The page table.
 * @param startAddr The start address to begin searching (inclusive).
 * @param endAddr The end address of the search range (exclusive).
 * @param pageAmount The number of consecutive unmapped pages needed.
 * @param outAddr Will be filled with the start address of the unmapped region if found.
 * @return On success, `0`. If no suitable region is found, `ERR`.
 */
static inline uint64_t page_table_find_unmapped_region(page_table_t* table, void* startAddr, void* endAddr,
    uint64_t pageAmount, void** outAddr)
{
    uintptr_t currentAddr = ROUND_DOWN((uintptr_t)startAddr, PAGE_SIZE);
    uintptr_t end = (uintptr_t)endAddr;

    if (pageAmount >= (PML3_SIZE / PAGE_SIZE))
    {
        while (currentAddr < end)
        {
            pml_index_t idx4 = PML_ADDR_TO_INDEX(currentAddr, PML4);
            pml_index_t idx3 = PML_ADDR_TO_INDEX(currentAddr, PML3);

            pml_entry_t* entry4 = &table->pml4->entries[idx4];
            if (!entry4->present)
            {
                *outAddr = (void*)currentAddr;
                return 0;
            }

            pml_t* pml3 = (pml_t*)pml_accessible_addr(*entry4);
            pml_entry_t* entry3 = &pml3->entries[idx3];

            if (!entry3->present)
            {
                *outAddr = (void*)currentAddr;
                return 0;
            }

            currentAddr = ROUND_UP(currentAddr + 1, PML3_SIZE);
        }
        return ERR;
    }

    if (pageAmount >= (PML2_SIZE / PAGE_SIZE))
    {
        while (currentAddr < end)
        {
            pml_index_t idx4 = PML_ADDR_TO_INDEX(currentAddr, PML4);
            pml_entry_t* entry4 = &table->pml4->entries[idx4];

            if (!entry4->present)
            {
                *outAddr = (void*)currentAddr;
                return 0;
            }

            pml_t* pml3 = (pml_t*)pml_accessible_addr(*entry4);
            pml_index_t idx3 = PML_ADDR_TO_INDEX(currentAddr, PML3);
            pml_entry_t* entry3 = &pml3->entries[idx3];

            if (!entry3->present)
            {
                *outAddr = (void*)currentAddr;
                return 0;
            }

            pml_t* pml2 = (pml_t*)pml_accessible_addr(*entry3);
            pml_index_t idx2 = PML_ADDR_TO_INDEX(currentAddr, PML2);
            pml_entry_t* entry2 = &pml2->entries[idx2];

            if (!entry2->present)
            {
                *outAddr = (void*)currentAddr;
                return 0;
            }

            currentAddr = ROUND_UP(currentAddr + 1, PML2_SIZE);
        }
        return ERR;
    }

    uintptr_t regionStart = 0;
    uint64_t consecutiveUnmapped = 0;

    while (currentAddr < end)
    {
        pml_index_t idx4 = PML_ADDR_TO_INDEX(currentAddr, PML4);
        pml_entry_t* entry4 = &table->pml4->entries[idx4];

        if (!entry4->present)
        {
            if (consecutiveUnmapped == 0)
            {
                regionStart = currentAddr;
            }

            uintptr_t skipTo = PML_INDEX_TO_ADDR(idx4 + 1, PML4);
            uint64_t skippedPages = (MIN(skipTo, end) - currentAddr) / PAGE_SIZE;
            consecutiveUnmapped += skippedPages;

            if (consecutiveUnmapped >= pageAmount)
            {
                *outAddr = (void*)regionStart;
                return 0;
            }

            currentAddr = skipTo;
            continue;
        }

        pml_t* pml3 = (pml_t*)pml_accessible_addr(*entry4);
        pml_index_t idx3 = PML_ADDR_TO_INDEX(currentAddr, PML3);
        pml_entry_t* entry3 = &pml3->entries[idx3];

        if (!entry3->present)
        {
            if (consecutiveUnmapped == 0)
            {
                regionStart = currentAddr;
            }

            uint64_t skippedPages = PML3_SIZE / PAGE_SIZE;
            consecutiveUnmapped += skippedPages;

            if (consecutiveUnmapped >= pageAmount)
            {
                *outAddr = (void*)regionStart;
                return 0;
            }

            currentAddr = ROUND_UP(currentAddr + 1, PML3_SIZE);
            continue;
        }

        pml_t* pml2 = (pml_t*)pml_accessible_addr(*entry3);
        pml_index_t idx2 = PML_ADDR_TO_INDEX(currentAddr, PML2);
        pml_entry_t* entry2 = &pml2->entries[idx2];

        if (!entry2->present)
        {
            if (consecutiveUnmapped == 0)
            {
                regionStart = currentAddr;
            }

            uint64_t skippedPages = PML2_SIZE / PAGE_SIZE;
            consecutiveUnmapped += skippedPages;

            if (consecutiveUnmapped >= pageAmount)
            {
                *outAddr = (void*)regionStart;
                return 0;
            }

            currentAddr = ROUND_UP(currentAddr + 1, PML2_SIZE);
            continue;
        }

        pml_t* pml1 = (pml_t*)pml_accessible_addr(*entry2);
        pml_index_t idx1 = PML_ADDR_TO_INDEX(currentAddr, PML1);

        for (; idx1 < PML_INDEX_AMOUNT && currentAddr < end; idx1++, currentAddr += PAGE_SIZE)
        {
            if (!pml1->entries[idx1].present)
            {
                if (consecutiveUnmapped == 0)
                {
                    regionStart = currentAddr;
                }
                consecutiveUnmapped++;

                if (consecutiveUnmapped >= pageAmount)
                {
                    *outAddr = (void*)regionStart;
                    return 0;
                }
            }
            else
            {
                consecutiveUnmapped = 0;
            }
        }
    }

    return ERR;
}

/**
 * @brief Pins a range of pages in the page table by increasing their pin depth.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pageAmount The number of pages to pin.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline uint64_t page_table_pin(page_table_t* table, const void* virtAddr, uint64_t pageAmount)
{
    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr + i * PAGE_SIZE, PML_NONE))
        {
            return ERR;
        }

        if (!traverse.entry->present)
        {
            return ERR;
        }

        if (traverse.entry->pinDepth == PML_PIN_DEPTH_MAX)
        {
            return ERR;
        }

        traverse.entry->pinDepth++;
    }

    return 0;
}

/**
 * @brief Unpins a range of pages in the page table by decrementing their pin depth.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pageAmount The number of pages to unpin.
 * @return On success, `0`. On failure, `ERR`.
 */
static inline void page_table_unpin(page_table_t* table, const void* virtAddr, uint64_t pageAmount)
{
    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            continue;
        }

        if (traverse.entry->pinDepth == 0)
        {
            continue;
        }

        traverse.entry->pinDepth--;
    }
}

/**
 * @brief Retrieves the maximum pin depth among a range of pages.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pageAmount The number of pages to check.
 * @return The maximum pin depth found in the range.
 */
static inline uint64_t page_table_get_max_pin_depth(page_table_t* table, const void* virtAddr, uint64_t pageAmount)
{
    uint64_t maxPinDepth = 0;

    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            continue;
        }

        if (traverse.entry->pinDepth > maxPinDepth)
        {
            maxPinDepth = traverse.entry->pinDepth;
        }
    }

    return maxPinDepth;
}

/**
 * @brief Checks if any page in a range is pinned.
 *
 * @param table The page table.
 * @param virtAddr The starting virtual address.
 * @param pageAmount The number of pages to check.
 * @return `true` if any page in the range us pinned, `false` otherwise.
 */
static inline bool page_table_is_pinned(page_table_t* table, const void* virtAddr, uint64_t pageAmount)
{
    page_table_traverse_t traverse = {0};
    page_table_traverse_init(&traverse);
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        if (!page_table_traverse(table, &traverse, (uintptr_t)virtAddr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            continue;
        }

        if (traverse.entry->pinDepth != 0)
        {
            return true;
        }
    }

    return false;
}

/** @} */
