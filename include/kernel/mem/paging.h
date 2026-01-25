#pragma once

#include <kernel/cpu/regs.h>
#include <kernel/mem/paging_types.h>

#include <_libstd/PAGE_SIZE.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * @addtogroup kernel_mem_paging
 * @{
 */

/**
 * @brief Invalidates a region of pages in the TLB.
 *
 * Even if a page table entry is modified, the CPU might still use a cached version of the entry in the TLB. To ensure
 * our changes are detected we must invalidate this cache using `invlpg` or if many pages are changed, a full TLB flush
 * by reloading CR3.
 *
 * @param addr The starting virtual address of the region.
 * @param amount The number of pages to invalidate.
 */
static inline void tlb_invalidate(void* addr, size_t amount)
{
    if (amount == 0)
    {
        return;
    }

    if (amount > 16)
    {
        cr3_write(cr3_read());
    }
    else
    {
        for (uint64_t i = 0; i < amount; i++)
        {
            ASM("invlpg (%0)" ::"r"(addr + (i * PAGE_SIZE)) : "memory");
        }
    }
}

/**
 * @brief Checks if a page table level is empty (all entries are 0).
 *
 * Used as a helper for `page_table_clear()`.
 *
 * @param pml The page table level to check.
 * @return true if all entries are raw 0, false otherwise.
 */
static inline bool pml_is_empty(pml_t* pml)
{
    for (pml_index_t i = 0; i < PML_INDEX_AMOUNT; i++)
    {
        if (pml->entries[i].raw != 0)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Allocates and initializes a new page table level.
 *
 * @param table The page table.
 * @param outPml Will be filled with the newly allocated page table level.
 * @return `true` if the pml was allocated, `false` otherwise.
 */
static inline bool pml_new(page_table_t* table, pml_t** outPml)
{
    pfn_t pfn;
    if (!table->allocPages(&pfn, 1))
    {
        return false;
    }
    pml_t* pml = PFN_TO_VIRT(pfn);
    memset(pml, 0, PAGE_SIZE);
    *outPml = pml;
    return true;
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
            pml_free(table, PFN_TO_VIRT(entry->pfn), level - 1);
        }
        else if (entry->owned)
        {
            pfn_t pfn = entry->pfn;
            table->freePages(&pfn, 1);
        }
    }

    pfn_t pfn = VIRT_TO_PFN(pml);
    table->freePages(&pfn, 1);
}

/**
 * @brief Initializes a page table.
 *
 * @param table The page table to initialize.
 * @param allocPages The function to use for allocating pages.
 * @param freePages The function to use for freeing pages.
 * @return `true` on success, `false` otherwise.
 */
static inline uint64_t page_table_init(page_table_t* table, pml_alloc_pages_t allocPages, pml_free_pages_t freePages)
{
    table->allocPages = allocPages;
    table->freePages = freePages;
    return pml_new(table, &table->pml4);
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
 * set, it returns `_FAIL`.
 *
 * @param table The page table.
 * @param current The current page table level.
 * @param index The index within the current page table level.
 * @param flags The flags to assign to a newly allocated page table level, if applicable.
 * @param out Will be filled with the retrieved or newly allocated page table level.
 * @return `true` if the pml was retrieved, `false` otherwise.
 */
static inline bool page_table_get_pml(page_table_t* table, pml_t* current, pml_index_t index, pml_flags_t flags,
    pml_t** out)
{
    pml_entry_t* entry = &current->entries[index];
    if (entry->present)
    {
        *out = PFN_TO_VIRT(entry->pfn);
        return true;
    }

    if (flags & PML_PRESENT)
    {
        pml_t* next;
        if (!pml_new(table, &next))
        {
            return false;
        }
        current->entries[index].raw = flags & PML_FLAGS_MASK;
        current->entries[index].pfn = VIRT_TO_PFN(next);
        *out = next;
        return true;
    }

    return false;
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

/**
 * @brief Create a `page_table_traverse_t` initializer.
 *
 * @return A `page_table_traverse_t` initializer.
 */
#define PAGE_TABLE_TRAVERSE_CREATE \
    { \
        .pml3Valid = false, \
        .pml2Valid = false, \
        .pml1Valid = false, \
    }

/**
 * @brief Allows for fast traversal of the page table by caching previously accessed layers.
 *
 * If the present flag is not set in `flags` then no new levels will be allocated and if non present pages are
 * encountered the function will return `false`.
 *
 * Note that higher level flags are or'd with `PML_WRITE | PML_USER` since only the permissions of a higher level will
 * apply to lower levels, meaning that the lowest level should be the one with the actual desired permissions.
 * Additionally, the `PML_GLOBAL` flag is not allowed on the PML3 level.
 *
 * @param table The page table.
 * @param traverse The helper structure used to cache each layer.
 * @param addr The target virtual address.
 * @param flags The flags to assigned to newly allocated levels, if the present flag is not set then dont allocate new
 * levels.
 * @return `true` if the page table was traversed, `false` otherwise.
 */
static inline bool page_table_traverse(page_table_t* table, page_table_traverse_t* traverse, const void* addr,
    pml_flags_t flags)
{
    pml_index_t newIdx3 = PML_ADDR_TO_INDEX(addr, PML4);
    if (!traverse->pml3Valid || traverse->oldIdx3 != newIdx3)
    {
        if (!page_table_get_pml(table, table->pml4, newIdx3, (flags | PML_WRITE | PML_USER) & ~PML_GLOBAL,
                &traverse->pml3))
        {
            return false;
        }
        traverse->oldIdx3 = newIdx3;
        traverse->pml2Valid = false; // Invalidate cache for lower levels
    }

    pml_index_t newIdx2 = PML_ADDR_TO_INDEX(addr, PML3);
    if (!traverse->pml2Valid || traverse->oldIdx2 != newIdx2)
    {
        if (!page_table_get_pml(table, traverse->pml3, newIdx2, flags | PML_WRITE | PML_USER, &traverse->pml2))
        {
            return false;
        }
        traverse->oldIdx2 = newIdx2;
        traverse->pml1Valid = false; // Invalidate cache for lower levels
    }

    pml_index_t newIdx1 = PML_ADDR_TO_INDEX(addr, PML2);
    if (!traverse->pml1Valid || traverse->oldIdx1 != newIdx1)
    {
        if (!page_table_get_pml(table, traverse->pml2, newIdx1, flags | PML_WRITE | PML_USER, &traverse->pml1))
        {
            return false;
        }
        traverse->oldIdx1 = newIdx1;
    }

    traverse->entry = &traverse->pml1->entries[PML_ADDR_TO_INDEX(addr, PML1)];
    return true;
}

/**
 * @brief Retrieves the physical address mapped to a given virtual address.
 *
 * @param out Output pointer for the physical address.
 * @param table The page table.
 * @param addr The virtual address to look up.
 * @return `true`if the address is mapped and the physical address was retrieved, `false` otherwise.
 */
static inline bool page_table_get_phys_addr(phys_addr_t* out,page_table_t* table, void* addr)
{
    size_t offset = ((uintptr_t)addr) % PAGE_SIZE;
    addr = (void*)ROUND_DOWN(addr, PAGE_SIZE);

    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;

    if (!page_table_traverse(table, &traverse, addr, PML_NONE))
    {
        return false;
    }

    if (!traverse.entry->present)
    {
        return false;
    }

    *out = PFN_TO_PHYS(traverse.entry->pfn) + offset;
    return true;
}

/**
 * @brief Checks if a range of virtual addresses is completely mapped.
 *
 * If any page in the range is not mapped, the function returns `false`.
 *
 * @param table The page table.
 * @param addr The starting virtual address.
 * @param amount The number of pages to check.
 * @return `true` if the entire range is mapped, `false` otherwise.
 */
static inline bool page_table_is_mapped(page_table_t* table, const void* addr, size_t amount)
{
    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;
    for (uint64_t i = 0; i < amount; i++)
    {
        if (!page_table_traverse(table, &traverse, addr + i * PAGE_SIZE, PML_NONE))
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
 * @param addr The starting virtual address.
 * @param amount The number of pages to check.
 * @return `true` if the entire range is unmapped, `false` otherwise.
 */
static inline bool page_table_is_unmapped(page_table_t* table, void* addr, size_t amount)
{
    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;

    for (uint64_t i = 0; i < amount; i++)
    {
        if (!page_table_traverse(table, &traverse, addr + i * PAGE_SIZE, PML_NONE))
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
 * If any page in the range is already mapped, the function will fail and return `_FAIL`.
 *
 * @param table The page table.
 * @param addr The starting virtual address.
 * @param phys The starting physical address.
 * @param amount The number of pages to map.
 * @param flags The flags to set for the mapped pages. Must include `PML_PRESENT`.
 * @param callbackId The callback ID to associate with the mapped pages or `PML_CALLBACK_NONE`.
 * @return `true` if the pages were mapped, `false` otherwise.
 */
static inline bool page_table_map(page_table_t* table, void* addr, phys_addr_t phys, size_t amount,
    pml_flags_t flags, pml_callback_id_t callbackId)
{
    if (!(flags & PML_PRESENT))
    {
        return false;
    }

    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;

    for (uint64_t i = 0; i < amount; i++)
    {
        if (!page_table_traverse(table, &traverse, addr, flags))
        {
            return false;
        }

        if (traverse.entry->present)
        {
            return false;
        }

        traverse.entry->raw = flags;
        traverse.entry->pfn = PHYS_TO_PFN(phys);
        traverse.entry->lowCallbackId = callbackId & 1;
        traverse.entry->highCallbackId = callbackId >> 1;

        phys += PAGE_SIZE;
        addr += PAGE_SIZE;
    }

    return true;
}

/**
 * @brief Maps an array of physical pages to contiguous virtual addresses in the page table.
 *
 * @param table The page table.
 * @param addr The starting virtual address.
 * @param pfns Array of page frame numbers to map.
 * @param amount The number of pages in the array to map.
 * @param flags The flags to set for the mapped pages. Must include `PML_PRESENT`.
 * @param callbackId The callback ID to associate with the mapped pages or `PML_CALLBACK_NONE`.
 * @return `true` if the pages were mapped, `false` otherwise.
 */
static inline bool page_table_map_pages(page_table_t* table, void* addr, const pfn_t* pfns, size_t amount,
    pml_flags_t flags, pml_callback_id_t callbackId)
{
    if (!(flags & PML_PRESENT))
    {
        return false;
    }

    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;

    for (uint64_t i = 0; i < amount; i++)
    {
        if (!page_table_traverse(table, &traverse, addr, flags))
        {
            return false;
        }

        if (traverse.entry->present)
        {
            return false;
        }

        traverse.entry->raw = flags;
        traverse.entry->pfn = pfns[i];
        traverse.entry->lowCallbackId = callbackId & 1;
        traverse.entry->highCallbackId = callbackId >> 1;

        addr = (void*)((uintptr_t)addr + PAGE_SIZE);
    }

    return true;
}

/**
 * @brief Unmaps a range of virtual addresses from the page table.
 *
 * If a page is not currently mapped, it is skipped.
 *
 * Will NOT free owned pages, instead it only sets the present flag to 0. This is to help with TLB shootdowns where we
 * must unmap, wait for all CPUs to acknowledge the unmap, and only then free the pages. Use `page_table_clear()` to
 * free owned pages separately.
 *
 * @param table The page table.
 * @param addr The starting virtual address.
 * @param amount The number of pages to unmap.
 */
static inline void page_table_unmap(page_table_t* table, void* addr, size_t amount)
{
    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;

    for (uint64_t i = 0; i < amount; i++)
    {
        if (!page_table_traverse(table, &traverse, addr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            continue;
        }

        traverse.entry->present = 0;
    }

    tlb_invalidate(addr, amount);
}

/**
 * @brief Buffer of pages used to batch page frees.
 * @struct page_table_page_buffer_t
 */
typedef struct
{
    pfn_t pfns[PML_PAGE_BUFFER_SIZE];
    uint64_t amount;
} page_table_page_buffer_t;

/**
 * @brief Pushes a page table level onto the page buffer, freeing the buffer if full.
 *
 * Used as a helper for `page_table_clear()`.
 *
 * @param table The page table.
 * @param buffer The page buffer.
 * @param address The address to push.
 */
static inline void page_table_page_buffer_push(page_table_t* table, page_table_page_buffer_t* buffer, void* address)
{
    buffer->pfns[buffer->amount] = VIRT_TO_PFN(address);
    buffer->amount++;

    if (buffer->amount >= PML_PAGE_BUFFER_SIZE)
    {
        table->freePages(buffer->pfns, buffer->amount);
        buffer->amount = 0;
    }
}

/**
 * @brief Flushes the page buffer, freeing any remaining pages.
 *
 * Used as a helper for `page_table_clear()`.
 *
 * @param table The page table.
 * @param buffer The page buffer.
 */
static inline void page_table_page_buffer_flush(page_table_t* table, page_table_page_buffer_t* buffer)
{
    if (buffer->amount > 0)
    {
        table->freePages(buffer->pfns, buffer->amount);
        buffer->amount = 0;
    }
}

/**
 * @brief Clears any empty page table levels any time a pml1, pml2 or pml3 boundry is crossed.
 *
 * Used as a helper for `page_table_clear()`.
 *
 * @param table The page table.
 * @param prevTraverse The previous traverse state.
 * @param traverse The current traverse state.
 * @param pageBuffer The page buffer.
 */
static inline void page_table_clear_pml1_pml2_pml3(page_table_t* table, page_table_traverse_t* prevTraverse,
    page_table_traverse_t* traverse, page_table_page_buffer_t* pageBuffer)
{
    if (prevTraverse->pml1Valid && prevTraverse->pml1 != traverse->pml1 && pml_is_empty(prevTraverse->pml1))
    {
        page_table_page_buffer_push(table, pageBuffer, prevTraverse->pml1);
        prevTraverse->pml2->entries[prevTraverse->oldIdx1].raw = 0;
        if (prevTraverse->pml2Valid && prevTraverse->pml2 != traverse->pml2 && pml_is_empty(prevTraverse->pml2))
        {
            page_table_page_buffer_push(table, pageBuffer, prevTraverse->pml2);
            prevTraverse->pml3->entries[prevTraverse->oldIdx2].raw = 0;
            if (prevTraverse->pml3Valid && prevTraverse->pml3 != traverse->pml3 && pml_is_empty(prevTraverse->pml3))
            {
                page_table_page_buffer_push(table, pageBuffer, prevTraverse->pml3);
                table->pml4->entries[prevTraverse->oldIdx3].raw = 0;
            }
        }
    }
}

/**
 * @brief Clears page table entries in the specified range and frees any owned pages.
 *
 * Intended to be used in conjunction with `page_table_unmap()` to first unmap pages and then free any owned pages after
 * TLB shootdown is complete.
 *
 * Any still present or pinned entries will be skipped.
 *
 * All unskipped entries will be fully cleared (set to 0).
 *
 * @param table The page table.
 * @param addr The starting virtual address.
 * @param amount The number of pages to clear.
 */
static inline void page_table_clear(page_table_t* table, void* addr, size_t amount)
{
    page_table_page_buffer_t pageBuffer = {0};

    page_table_traverse_t prevTraverse = PAGE_TABLE_TRAVERSE_CREATE;
    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;
    for (uint64_t i = 0; i < amount; i++)
    {
        page_table_clear_pml1_pml2_pml3(table, &prevTraverse, &traverse, &pageBuffer);

        if (page_table_traverse(table, &traverse, addr + i * PAGE_SIZE, PML_NONE) == _FAIL)
        {
            prevTraverse.pml1Valid = false;
            prevTraverse.pml2Valid = false;
            prevTraverse.pml3Valid = false;
            continue;
        }
        prevTraverse = traverse;

        if (traverse.entry->present)
        {
            continue;
        }

        if (traverse.entry->owned)
        {
            page_table_page_buffer_push(table, &pageBuffer, PFN_TO_VIRT(traverse.entry->pfn));
        }

        traverse.entry->raw = 0;
    }

    page_table_clear_pml1_pml2_pml3(table, &prevTraverse, &traverse, &pageBuffer);
    page_table_page_buffer_flush(table, &pageBuffer);
}

/**
 * @brief Collects the number of pages associated with each callback ID in the specified range.
 *
 * @param table The page table.
 * @param addr The starting virtual address.
 * @param amount The number of pages to check.
 * @param callbacks An array of size `PML_MAX_CALLBACK` that will be filled with the occurrences of each callback ID.
 */
static inline void page_table_collect_callbacks(page_table_t* table, void* addr, size_t amount, uint64_t* callbacks)
{
    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;

    for (uint64_t i = 0; i < amount; i++)
    {
        if (page_table_traverse(table, &traverse, addr + i * PAGE_SIZE, PML_NONE) == _FAIL)
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            continue;
        }

        pml_callback_id_t callbackId = traverse.entry->lowCallbackId | (traverse.entry->highCallbackId << 1);
        if (callbackId != PML_CALLBACK_NONE)
        {
            callbacks[callbackId]++;
        }
    }
}

/**
 * @brief Sets the flags for a range of pages in the page table.
 *
 * If a page is not currently mapped, it is skipped.
 *
 * @param table The page table.
 * @param addr The starting virtual address.
 * @param amount The number of pages to update.
 * @param flags The new flags to set. The `PML_OWNED` flag is preserved.
 * @return `true` if the entire regions flags were updated, `false` otherwise.
 */
static inline bool page_table_set_flags(page_table_t* table, void* addr, size_t amount, pml_flags_t flags)
{
    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;

    for (size_t i = 0; i < amount; i++)
    {
        if (!page_table_traverse(table, &traverse, addr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            return false;
        }

        if (traverse.entry->owned)
        {
            flags |= PML_OWNED;
        }

        // Bit magic to only update the flags while preserving the address and callback ID.
        traverse.entry->raw = (traverse.entry->raw & ~PML_FLAGS_MASK) | (flags & PML_FLAGS_MASK);
    }

    tlb_invalidate(addr, amount);
    return true;
}

/**
 * @brief Finds the first contiguous unmapped region with the given number of pages within the specified address range.
 *
 * Good luck with this function, im like 99% sure it works.
 *
 * This function should be `O(r)` in the worse case where `r` is the amount of pages in the address range, note how the
 * number of pages needed does not affect the complexity. This has the fun affect that the more memory is allocated the
 * faster this function will run on average.
 *
 * @param table The page table.
 * @param startAddr The start address to begin searching (inclusive).
 * @param endAddr The end address of the search range (exclusive).
 * @param amount The number of consecutive unmapped pages needed.
 * @param alignment The required alignment for the region in bytes.
 * @param outAddr Will be filled with the start address of the unmapped region if found.
 * @return `true` if a suitable region was found, `false` otherwise.
 */
static inline bool page_table_find_unmapped_region(page_table_t* table, uintptr_t startAddr, uintptr_t endAddr,
    size_t amount, size_t alignment, void** outAddr)
{
    uintptr_t currentAddr = ROUND_DOWN(startAddr, PAGE_SIZE);
    uintptr_t end = endAddr;

    if (alignment < PAGE_SIZE)
    {
        alignment = PAGE_SIZE;
    }

    if (amount >= (PML3_SIZE / PAGE_SIZE))
    {
        while (currentAddr < end)
        {
            pml_index_t idx4 = PML_ADDR_TO_INDEX(currentAddr, PML4);
            pml_index_t idx3 = PML_ADDR_TO_INDEX(currentAddr, PML3);

            pml_entry_t* entry4 = &table->pml4->entries[idx4];
            if (!entry4->present)
            {
                uintptr_t alignedAddr = ROUND_UP(currentAddr, alignment);
                uintptr_t nextPml4 = ROUND_UP(currentAddr + 1, PML4_SIZE);
                if (alignedAddr < nextPml4 && alignedAddr < end)
                {
                    *outAddr = (void*)alignedAddr;
                    return true;
                }
                currentAddr = nextPml4;
                continue;
            }

            pml_t* pml3 = PFN_TO_VIRT(entry4->pfn);
            pml_entry_t* entry3 = &pml3->entries[idx3];

            if (!entry3->present)
            {
                uintptr_t alignedAddr = ROUND_UP(currentAddr, alignment);
                uintptr_t nextPml3 = ROUND_UP(currentAddr + 1, PML3_SIZE);
                if (alignedAddr < nextPml3 && alignedAddr < end)
                {
                    *outAddr = (void*)alignedAddr;
                    return true;
                }
            }

            currentAddr = ROUND_UP(currentAddr + 1, PML3_SIZE);
        }

        return false;
    }

    if (amount >= (PML2_SIZE / PAGE_SIZE))
    {
        while (currentAddr < end)
        {
            pml_index_t idx4 = PML_ADDR_TO_INDEX(currentAddr, PML4);
            pml_entry_t* entry4 = &table->pml4->entries[idx4];

            if (!entry4->present)
            {
                uintptr_t alignedAddr = ROUND_UP(currentAddr, alignment);
                uintptr_t nextPml4 = ROUND_UP(currentAddr + 1, PML4_SIZE);
                if (alignedAddr < nextPml4 && alignedAddr < end)
                {
                    *outAddr = (void*)alignedAddr;
                    return true;
                }
                currentAddr = nextPml4;
                continue;
            }

            pml_t* pml3 = PFN_TO_VIRT(entry4->pfn);
            pml_index_t idx3 = PML_ADDR_TO_INDEX(currentAddr, PML3);
            pml_entry_t* entry3 = &pml3->entries[idx3];

            if (!entry3->present)
            {
                uintptr_t alignedAddr = ROUND_UP(currentAddr, alignment);
                uintptr_t nextPml3 = ROUND_UP(currentAddr + 1, PML3_SIZE);
                if (alignedAddr < nextPml3 && alignedAddr < end)
                {
                    *outAddr = (void*)alignedAddr;
                    return true;
                }
                currentAddr = nextPml3;
                continue;
            }

            pml_t* pml2 = PFN_TO_VIRT(entry3->pfn);
            pml_index_t idx2 = PML_ADDR_TO_INDEX(currentAddr, PML2);
            pml_entry_t* entry2 = &pml2->entries[idx2];

            if (!entry2->present)
            {
                uintptr_t alignedAddr = ROUND_UP(currentAddr, alignment);
                uintptr_t nextPml2 = ROUND_UP(currentAddr + 1, PML2_SIZE);
                if (alignedAddr < nextPml2 && alignedAddr < end)
                {
                    *outAddr = (void*)alignedAddr;
                    return true;
                }
            }

            currentAddr = ROUND_UP(currentAddr + 1, PML2_SIZE);
        }

        return false;
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

            if (consecutiveUnmapped >= amount)
            {
                *outAddr = (void*)regionStart;
                return true;
            }

            currentAddr = skipTo;
            continue;
        }

        pml_t* pml3 = PFN_TO_VIRT(entry4->pfn);
        pml_index_t idx3 = PML_ADDR_TO_INDEX(currentAddr, PML3);
        pml_entry_t* entry3 = &pml3->entries[idx3];

        if (!entry3->present)
        {
            uintptr_t skipTo = ROUND_UP(currentAddr + 1, PML3_SIZE);

            if (consecutiveUnmapped == 0)
            {
                uintptr_t alignedAddr = ROUND_UP(currentAddr, alignment);
                if (alignedAddr < skipTo && alignedAddr < end)
                {
                    regionStart = alignedAddr;
                    consecutiveUnmapped = (MIN(skipTo, end) - alignedAddr) / PAGE_SIZE;
                }
            }
            else
            {
                uint64_t skippedPages = (MIN(skipTo, end) - currentAddr) / PAGE_SIZE;
                consecutiveUnmapped += skippedPages;
            }

            if (consecutiveUnmapped >= amount)
            {
                *outAddr = (void*)regionStart;
                return true;
            }

            currentAddr = skipTo;
            continue;
        }

        pml_t* pml2 = PFN_TO_VIRT(entry3->pfn);
        pml_index_t idx2 = PML_ADDR_TO_INDEX(currentAddr, PML2);
        pml_entry_t* entry2 = &pml2->entries[idx2];

        if (!entry2->present)
        {
            uintptr_t skipTo = ROUND_UP(currentAddr + 1, PML2_SIZE);

            if (consecutiveUnmapped == 0)
            {
                uintptr_t alignedAddr = ROUND_UP(currentAddr, alignment);
                if (alignedAddr < skipTo && alignedAddr < end)
                {
                    regionStart = alignedAddr;
                    consecutiveUnmapped = (MIN(skipTo, end) - alignedAddr) / PAGE_SIZE;
                }
            }
            else
            {
                uint64_t skippedPages = (MIN(skipTo, end) - currentAddr) / PAGE_SIZE;
                consecutiveUnmapped += skippedPages;
            }

            if (consecutiveUnmapped >= amount)
            {
                *outAddr = (void*)regionStart;
                return true;
            }

            currentAddr = skipTo;
            continue;
        }

        pml_t* pml1 = PFN_TO_VIRT(entry2->pfn);
        pml_index_t idx1 = PML_ADDR_TO_INDEX(currentAddr, PML1);

        for (; idx1 < PML_INDEX_AMOUNT && currentAddr < end; idx1++, currentAddr += PAGE_SIZE)
        {
            if (!pml1->entries[idx1].present)
            {
                if (consecutiveUnmapped == 0)
                {
                    uintptr_t alignedAddr = ROUND_UP(currentAddr, alignment);
                    if (alignedAddr == currentAddr)
                    {
                        regionStart = currentAddr;
                        consecutiveUnmapped++;
                    }
                }
                else
                {
                    consecutiveUnmapped++;
                }

                if (consecutiveUnmapped >= amount)
                {
                    *outAddr = (void*)regionStart;
                    return true;
                }
            }
            else
            {
                consecutiveUnmapped = 0;
            }
        }
    }

    return false;
}

/**
 * @brief Checks if any page in a range is pinned.
 *
 * @param table The page table.
 * @param addr The starting virtual address.
 * @param amount The number of pages to check.
 * @return `true` if any page in the range us pinned, `false` otherwise.
 */
static inline bool page_table_is_pinned(page_table_t* table, void* addr, size_t amount)
{
    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;
    for (uint64_t i = 0; i < amount; i++)
    {
        if (!page_table_traverse(table, &traverse, addr + i * PAGE_SIZE, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            continue;
        }

        if (traverse.entry->pinned)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief Counts the number of pages in a range that have all the specified flags set.
 *
 * Can be used to, for example, check the total amount of pages allocated to a process by counting the pages with the
 * `PML_PRESENT | PML_USER | PML_OWNED` flags set.
 *
 * @param table The page table.
 * @param addr The starting virtual address.
 * @param amount The number of pages to check.
 * @param flags The flags to check for.
 * @return The number of pages with the specified flags set.
 */
static inline uint64_t page_table_count_pages_with_flags(page_table_t* table, void* addr, size_t amount,
    pml_flags_t flags)
{
    uint64_t count = 0;
    while (amount > 0)
    {
        pml_index_t idx4 = PML_ADDR_TO_INDEX((uintptr_t)addr, PML4);
        pml_entry_t* entry4 = &table->pml4->entries[idx4];

        if (!entry4->present)
        {
            uint64_t skipPages = MIN(amount, (PML_INDEX_TO_ADDR(idx4 + 1, PML4) - (uintptr_t)addr) / PAGE_SIZE);
            addr = (void*)((uintptr_t)addr + skipPages * PAGE_SIZE);
            amount -= skipPages;
            continue;
        }

        pml_t* pml3 = PFN_TO_VIRT(entry4->pfn);
        pml_index_t idx3 = PML_ADDR_TO_INDEX((uintptr_t)addr, PML3);
        pml_entry_t* entry3 = &pml3->entries[idx3];

        if (!entry3->present)
        {
            uint64_t skipPages = MIN(amount, (PML_INDEX_TO_ADDR(idx3 + 1, PML3) - (uintptr_t)addr) / PAGE_SIZE);
            addr = (void*)((uintptr_t)addr + skipPages * PAGE_SIZE);
            amount -= skipPages;
            continue;
        }

        pml_t* pml2 = PFN_TO_VIRT(entry3->pfn);
        pml_index_t idx2 = PML_ADDR_TO_INDEX((uintptr_t)addr, PML2);
        pml_entry_t* entry2 = &pml2->entries[idx2];

        if (!entry2->present)
        {
            uint64_t skipPages = MIN(amount, (PML_INDEX_TO_ADDR(idx2 + 1, PML2) - (uintptr_t)addr) / PAGE_SIZE);
            addr = (void*)((uintptr_t)addr + skipPages * PAGE_SIZE);
            amount -= skipPages;
            continue;
        }

        pml_t* pml1 = PFN_TO_VIRT(entry2->pfn);
        pml_index_t idx1 = PML_ADDR_TO_INDEX((uintptr_t)addr, PML1);

        for (; idx1 < PML_INDEX_AMOUNT && amount > 0; idx1++, addr = (void*)((uintptr_t)addr + PAGE_SIZE), amount--)
        {
            pml_entry_t* entry1 = &pml1->entries[idx1];
            if (!entry1->present)
            {
                continue;
            }
            if ((entry1->raw & flags) == flags)
            {
                count++;
            }
        }
    }

    return count;
}

/** @} */
