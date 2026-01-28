#include <kernel/cpu/ipi.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/space.h>

#include <kernel/cpu/cpu.h>
#include <kernel/log/panic.h>
#include <kernel/mem/paging.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/space.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/clock.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/map.h>
#include <sys/math.h>
#include <sys/proc.h>

static bool space_pmm_bitmap_alloc_pages(pfn_t* pfns, size_t pageAmount)
{
    for (size_t i = 0; i < pageAmount; i++)
    {
        pfn_t pfn = pmm_alloc_bitmap(1, UINT32_MAX, 0);
        if (pfn == PFN_INVALID)
        {
            for (size_t j = 0; j < i; j++)
            {
                pmm_free(pfns[j]);
            }
            return false;
        }
        pfns[i] = pfn;
    }
    return true;
}

static inline void space_map_kernel_space_region(space_t* space, uintptr_t start, uintptr_t end)
{
    space_t* kernelSpace = vmm_kernel_space_get();
    assert(kernelSpace != NULL);

    pml_index_t startIndex = PML_ADDR_TO_INDEX(start, PML4);
    pml_index_t endIndex = PML_ADDR_TO_INDEX(end - 1, PML4) + 1; // Inclusive end

    for (pml_index_t i = startIndex; i < endIndex; i++)
    {
        space->pageTable.pml4->entries[i] = kernelSpace->pageTable.pml4->entries[i];
        space->pageTable.pml4->entries[i].owned = 0;
    }
}

static inline void space_unmap_kernel_space_region(space_t* space, uintptr_t start, uintptr_t end)
{
    pml_index_t startIndex = PML_ADDR_TO_INDEX(start, PML4);
    pml_index_t endIndex = PML_ADDR_TO_INDEX(end - 1, PML4) + 1; // Inclusive end

    for (pml_index_t i = startIndex; i < endIndex; i++)
    {
        space->pageTable.pml4->entries[i].raw = 0;
    }
}

static bool space_pinned_page_cmp(map_entry_t* entry, const void* key)
{
    space_pinned_page_t* page = CONTAINER_OF(entry, space_pinned_page_t, mapEntry);
    return page->address == (uintptr_t)key;
}

status_t space_init(space_t* space, uintptr_t startAddress, uintptr_t endAddress, space_flags_t flags)
{
    if (space == NULL)
    {
        return ERR(MMU, INVAL);
    }

    if (flags & SPACE_USE_PMM_BITMAP)
    {
        if (!page_table_init(&space->pageTable, space_pmm_bitmap_alloc_pages, pmm_free_pages))
        {
            return ERR(MMU, NOMEM);
        }
        // We only use the bitmap pmm allocator for the page table itself, not for mappings.
        space->pageTable.allocPages = pmm_alloc_pages;
    }
    else
    {
        if (!page_table_init(&space->pageTable, pmm_alloc_pages, pmm_free_pages))
        {
            return ERR(MMU, NOMEM);
        }
    }

    MAP_DEFINE_INIT(space->pinnedPages, space_pinned_page_cmp);
    space->startAddress = startAddress;
    space->endAddress = endAddress;
    space->freeAddress = startAddress;
    space->flags = flags;
    space->callbacks = NULL;
    space->callbacksLength = 0;
    BITMAP_DEFINE_INIT(space->callbackBitmap, PML_MAX_CALLBACK);
    BITMAP_DEFINE_INIT(space->cpus, CPU_MAX);
    atomic_init(&space->shootdownAcks, 0);
    lock_init(&space->lock);

    if (flags & SPACE_MAP_KERNEL_BINARY)
    {
        space_map_kernel_space_region(space, VMM_KERNEL_BINARY_MIN, VMM_KERNEL_BINARY_MAX);
    }

    if (flags & SPACE_MAP_KERNEL_HEAP)
    {
        space_map_kernel_space_region(space, VMM_KERNEL_HEAP_MIN, VMM_KERNEL_HEAP_MAX);
    }

    if (flags & SPACE_MAP_IDENTITY)
    {
        space_map_kernel_space_region(space, VMM_IDENTITY_MAPPED_MIN, VMM_IDENTITY_MAPPED_MAX);
    }

    return OK;
}

void space_deinit(space_t* space)
{
    if (space == NULL)
    {
        return;
    }

    if (!bitmap_is_empty(&space->cpus))
    {
        panic(NULL, "Attempted to free address space still in use by CPUs");
    }

    uint64_t index;
    BITMAP_FOR_EACH_SET(&index, &space->callbackBitmap)
    {
        space->callbacks[index].func(space->callbacks[index].data);
    }

    if (space->flags & SPACE_MAP_KERNEL_BINARY)
    {
        space_unmap_kernel_space_region(space, VMM_KERNEL_BINARY_MIN, VMM_KERNEL_BINARY_MAX);
    }

    if (space->flags & SPACE_MAP_KERNEL_HEAP)
    {
        space_unmap_kernel_space_region(space, VMM_KERNEL_HEAP_MIN, VMM_KERNEL_HEAP_MAX);
    }

    if (space->flags & SPACE_MAP_IDENTITY)
    {
        space_unmap_kernel_space_region(space, VMM_IDENTITY_MAPPED_MIN, VMM_IDENTITY_MAPPED_MAX);
    }

    free(space->callbacks);
    page_table_deinit(&space->pageTable);
}

static void space_align_region(void** virtAddr, size_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*virtAddr, PAGE_SIZE);
    *length += ((uintptr_t)*virtAddr - (uintptr_t)aligned);
    *virtAddr = aligned;
}

static status_t space_populate_user_region(space_t* space, const void* buffer, size_t pageAmount)
{
    for (size_t i = 0; i < pageAmount; i++)
    {
        uintptr_t addr = (uintptr_t)buffer + (i * PAGE_SIZE);
        if (page_table_is_mapped(&space->pageTable, (void*)addr, 1))
        {
            continue;
        }

        pfn_t pfn = pmm_alloc();
        if (pfn == PFN_INVALID)
        {
            return ERR(MMU, NOMEM);
        }

        if (!page_table_map(&space->pageTable, (void*)addr, PFN_TO_PHYS(pfn), 1,
                PML_PRESENT | PML_USER | PML_WRITE | PML_OWNED, PML_CALLBACK_NONE))
        {
            pmm_free(pfn);
            return ERR(MMU, NOMEM);
        }
    }

    return OK;
}

static void space_pin_depth_dec(space_t* space, const void* address, uint64_t amount)
{
    address = (void*)ROUND_DOWN((uintptr_t)address, PAGE_SIZE);

    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;
    for (uint64_t i = 0; i < amount; i++)
    {
        const void* addr = address + (i * PAGE_SIZE);
        if (!page_table_traverse(&space->pageTable, &traverse, addr, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present || !traverse.entry->pinned)
        {
            continue;
        }

        uint64_t hash = hash_uint64((uintptr_t)addr);
        map_entry_t* entry = map_find(&space->pinnedPages, addr, hash);
        if (entry == NULL) // Not pinned more then once
        {
            traverse.entry->pinned = false;
            continue;
        }

        space_pinned_page_t* pinnedPage = CONTAINER_OF(entry, space_pinned_page_t, mapEntry);
        pinnedPage->pinCount--;
        if (pinnedPage->pinCount == 0)
        {
            map_remove(&space->pinnedPages, &pinnedPage->mapEntry, hash);
            free(pinnedPage);
            traverse.entry->pinned = false;
        }
    }
}

static inline status_t space_pin_depth_inc(space_t* space, const void* address, uint64_t amount)
{
    address = (void*)ROUND_DOWN((uintptr_t)address, PAGE_SIZE);

    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;
    for (uint64_t i = 0; i < amount; i++)
    {
        const void* addr = address + (i * PAGE_SIZE);
        if (!page_table_traverse(&space->pageTable, &traverse, addr, PML_NONE))
        {
            continue;
        }

        if (!traverse.entry->present)
        {
            continue;
        }

        if (!traverse.entry->pinned)
        {
            traverse.entry->pinned = true;
            continue;
        }

        uint64_t hash = hash_uint64((uintptr_t)addr);
        map_entry_t* entry = map_find(&space->pinnedPages, addr, hash);
        if (entry != NULL) // Already pinned more than once
        {
            space_pinned_page_t* pinnedPage = CONTAINER_OF(entry, space_pinned_page_t, mapEntry);
            pinnedPage->pinCount++;
            continue;
        }

        space_pinned_page_t* newPinnedPage = malloc(sizeof(space_pinned_page_t));
        if (newPinnedPage == NULL)
        {
            space_pin_depth_dec(space, address, i);
            return ERR(MMU, NOMEM);
        }
        map_entry_init(&newPinnedPage->mapEntry);
        newPinnedPage->address = (uintptr_t)addr;
        newPinnedPage->pinCount = 2; // One for the page table, one for the map
        map_insert(&space->pinnedPages, &newPinnedPage->mapEntry, hash);
    }

    return OK;
}

status_t space_pin(space_t* space, const void* buffer, size_t length, stack_pointer_t* userStack)
{
    if (space == NULL || (buffer == NULL && length != 0))
    {
        return ERR(MMU, INVAL);
    }

    if (length == 0)
    {
        return OK;
    }

    uintptr_t bufferOverflow = (uintptr_t)buffer + length;
    if (bufferOverflow < (uintptr_t)buffer)
    {
        return ERR(MMU, TOOBIG);
    }

    LOCK_SCOPE(&space->lock);

    space_align_region((void**)&buffer, &length);
    size_t pageAmount = BYTES_TO_PAGES(length);

    if (!page_table_is_mapped(&space->pageTable, buffer, pageAmount))
    {
        if (userStack == NULL || !stack_pointer_is_in_stack(userStack, (uintptr_t)buffer, length))
        {
            return ERR(MMU, FAULT);
        }

        status_t status = space_populate_user_region(space, buffer, pageAmount);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    status_t status = space_pin_depth_inc(space, buffer, pageAmount);
    if (IS_ERR(status))
    {
        return status;
    }

    return OK;
}

status_t space_pin_terminated(size_t* outPinned, space_t* space, const void* address, const void* terminator,
    size_t objectSize, size_t maxCount, stack_pointer_t* userStack)
{
    if (outPinned != NULL)
    {
        *outPinned = 0;
    }
    if (space == NULL || address == NULL || terminator == NULL || objectSize == 0 || maxCount == 0)
    {
        return ERR(MMU, INVAL);
    }

    size_t terminatorMatchedBytes = 0;
    uintptr_t current = (uintptr_t)address;
    uintptr_t end = (uintptr_t)address + (maxCount * objectSize);
    if (end < (uintptr_t)address)
    {
        return ERR(MMU, TOOBIG);
    }

    LOCK_SCOPE(&space->lock);

    status_t status;
    uint64_t pinnedPages = 0;
    while (current < end)
    {
        if (!page_table_is_mapped(&space->pageTable, (void*)current, 1))
        {
            if (userStack == NULL || !stack_pointer_is_in_stack(userStack, current, 1))
            {
                status = ERR(MMU, FAULT);
                goto error;
            }

            status = space_populate_user_region(space, (void*)ROUND_DOWN(current, PAGE_SIZE), 1);
            if (IS_ERR(status))
            {
                goto error;
            }
        }

        status = space_pin_depth_inc(space, (void*)current, 1);
        if (IS_ERR(status))
        {
            goto error;
        }
        pinnedPages++;

        uintptr_t scanEnd = MIN(ROUND_UP(current + 1, PAGE_SIZE), end);

        if (objectSize == 1)
        {
            uint8_t term = *(const uint8_t*)terminator;
            for (uintptr_t scanAddr = current; scanAddr < scanEnd; scanAddr++)
            {
                if (*(uint8_t*)scanAddr == term)
                {
                    if (outPinned != NULL)
                    {
                        *outPinned = scanAddr - (uintptr_t)address + 1;
                    }
                    return OK;
                }
            }
            current = scanEnd;
        }
        else
        {
            uintptr_t scanAddr = current;
            while (scanAddr < scanEnd)
            {
                if (*((uint8_t*)scanAddr) == ((uint8_t*)terminator)[terminatorMatchedBytes])
                {
                    terminatorMatchedBytes++;
                    if (terminatorMatchedBytes == objectSize)
                    {
                        if (outPinned != NULL)
                        {
                            *outPinned = scanAddr - (uintptr_t)address + 1;
                        }
                        return OK;
                    }
                    scanAddr++;
                }
                else
                {
                    scanAddr = scanAddr - terminatorMatchedBytes + objectSize;
                    terminatorMatchedBytes = 0;
                }
            }
            current = scanAddr;
        }
    }

    status = ERR(MMU, FAULT);
error:
    space_pin_depth_dec(space, address, pinnedPages);
    return status;
}

void space_unpin(space_t* space, const void* address, size_t length)
{
    if (space == NULL || (address == NULL && length != 0))
    {
        return;
    }

    if (length == 0)
    {
        return;
    }

    uintptr_t overflow = (uintptr_t)address + length;
    if (overflow < (uintptr_t)address)
    {
        return;
    }

    LOCK_SCOPE(&space->lock);

    space_align_region((void**)&address, &length);
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    space_pin_depth_dec(space, address, pageAmount);
}

bool space_check_access(space_t* space, const void* addr, size_t length)
{
    if (space == NULL || (addr == NULL && length != 0))
    {
        return false;
    }

    if (length == 0)
    {
        return true;
    }

    uintptr_t addrOverflow = (uintptr_t)addr + length;
    if (addrOverflow < (uintptr_t)addr)
    {
        return false;
    }

    if ((uintptr_t)addr < space->startAddress || addrOverflow > space->endAddress)
    {
        return false;
    }

    return true;
}

bool space_is_mapped(space_t* space, const void* virtAddr, size_t length)
{
    space_align_region((void**)&virtAddr, &length);
    LOCK_SCOPE(&space->lock);
    return page_table_is_mapped(&space->pageTable, virtAddr, BYTES_TO_PAGES(length));
}

uint64_t space_user_page_count(space_t* space)
{
    if (space == NULL)
    {
        return 0;
    }

    LOCK_SCOPE(&space->lock);
    return page_table_count_pages_with_flags(&space->pageTable, (void*)VMM_USER_SPACE_MIN,
        BYTES_TO_PAGES(VMM_USER_SPACE_MAX - VMM_USER_SPACE_MIN), PML_PRESENT | PML_USER | PML_OWNED);
}

status_t space_virt_to_phys(phys_addr_t* out, space_t* space, const void* virtAddr)
{
    if (space == NULL)
    {
        return ERR(MMU, INVAL);
    }

    LOCK_SCOPE(&space->lock);
    if (!page_table_get_phys_addr(out, &space->pageTable, (void*)virtAddr))
    {
        return ERR(MMU, FAULT);
    }

    return OK;
}