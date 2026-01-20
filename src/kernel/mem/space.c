#include <kernel/cpu/ipi.h>
#include <kernel/mem/space.h>

#include <kernel/cpu/cpu.h>
#include <kernel/log/panic.h>
#include <kernel/mem/paging.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/space.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/clock.h>
#include <kernel/utils/map.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>
#include <sys/proc.h>

static uint64_t space_pmm_bitmap_alloc_pages(pfn_t* pfns, size_t pageAmount)
{
    for (size_t i = 0; i < pageAmount; i++)
    {
        pfn_t pfn = pmm_alloc_bitmap(1, UINT32_MAX, 0);
        if (pfn == ERR)
        {
            for (size_t j = 0; j < i; j++)
            {
                pmm_free(pfns[j]);
            }
            return ERR;
        }
        pfns[i] = pfn;
    }
    return 0;
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

uint64_t space_init(space_t* space, uintptr_t startAddress, uintptr_t endAddress, space_flags_t flags)
{
    if (space == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (flags & SPACE_USE_PMM_BITMAP)
    {
        if (page_table_init(&space->pageTable, space_pmm_bitmap_alloc_pages, pmm_free_pages) == ERR)
        {
            errno = ENOMEM;
            return ERR;
        }
        // We only use the bitmap pmm allocator for the page table itself, not for mappings.
        space->pageTable.allocPages = pmm_alloc_pages;
    }
    else
    {
        if (page_table_init(&space->pageTable, pmm_alloc_pages, pmm_free_pages) == ERR)
        {
            errno = ENOMEM;
            return ERR;
        }
    }

    map_init(&space->pinnedPages);
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

    return 0;
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

static uint64_t space_populate_user_region(space_t* space, const void* buffer, size_t pageAmount)
{
    for (size_t i = 0; i < pageAmount; i++)
    {
        uintptr_t addr = (uintptr_t)buffer + (i * PAGE_SIZE);
        if (page_table_is_mapped(&space->pageTable, (void*)addr, 1))
        {
            continue;
        }

        pfn_t pfn = pmm_alloc();
        if (pfn == ERR)
        {
            return ERR;
        }

        if (page_table_map(&space->pageTable, (void*)addr, PFN_TO_PHYS(pfn), 1,
                PML_PRESENT | PML_USER | PML_WRITE | PML_OWNED, PML_CALLBACK_NONE) == ERR)
        {
            pmm_free(pfn);
            return ERR;
        }
    }

    return 0;
}

static void space_pin_depth_dec(space_t* space, const void* address, uint64_t amount)
{
    address = (void*)ROUND_DOWN((uintptr_t)address, PAGE_SIZE);

    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;
    for (uint64_t i = 0; i < amount; i++)
    {
        const void* addr = address + (i * PAGE_SIZE);
        if (page_table_traverse(&space->pageTable, &traverse, addr, PML_NONE) == ERR)
        {
            continue;
        }

        if (!traverse.entry->present || !traverse.entry->pinned)
        {
            continue;
        }

        map_key_t key = map_key_uint64((uintptr_t)addr);
        map_entry_t* entry = map_get(&space->pinnedPages, &key);
        if (entry == NULL) // Not pinned more then once
        {
            traverse.entry->pinned = false;
            continue;
        }

        space_pinned_page_t* pinnedPage = CONTAINER_OF(entry, space_pinned_page_t, mapEntry);
        pinnedPage->pinCount--;
        if (pinnedPage->pinCount == 0)
        {
            map_remove(&space->pinnedPages, &pinnedPage->mapEntry);
            free(pinnedPage);
            traverse.entry->pinned = false;
        }
    }
}

static inline uint64_t space_pin_depth_inc(space_t* space, const void* address, uint64_t amount)
{
    address = (void*)ROUND_DOWN((uintptr_t)address, PAGE_SIZE);

    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;
    for (uint64_t i = 0; i < amount; i++)
    {
        const void* addr = address + (i * PAGE_SIZE);
        if (page_table_traverse(&space->pageTable, &traverse, addr, PML_NONE) == ERR)
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

        map_key_t key = map_key_uint64((uintptr_t)addr);
        map_entry_t* entry = map_get(&space->pinnedPages, &key);
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
            return ERR;
        }
        map_entry_init(&newPinnedPage->mapEntry);
        newPinnedPage->pinCount = 2; // One for the page table, one for the map
        if (map_insert(&space->pinnedPages, &key, &newPinnedPage->mapEntry) == ERR)
        {
            free(newPinnedPage);
            space_pin_depth_dec(space, address, i);
            return ERR;
        }
    }

    return 0;
}

uint64_t space_pin(space_t* space, const void* buffer, size_t length, stack_pointer_t* userStack)
{
    if (space == NULL || (buffer == NULL && length != 0))
    {
        errno = EINVAL;
        return ERR;
    }

    if (length == 0)
    {
        return 0;
    }

    uintptr_t bufferOverflow = (uintptr_t)buffer + length;
    if (bufferOverflow < (uintptr_t)buffer)
    {
        errno = EOVERFLOW;
        return ERR;
    }

    LOCK_SCOPE(&space->lock);

    space_align_region((void**)&buffer, &length);
    size_t pageAmount = BYTES_TO_PAGES(length);

    if (!page_table_is_mapped(&space->pageTable, buffer, pageAmount))
    {
        if (userStack == NULL || !stack_pointer_is_in_stack(userStack, (uintptr_t)buffer, length))
        {
            errno = EFAULT;
            return ERR;
        }

        if (space_populate_user_region(space, buffer, pageAmount) == ERR)
        {
            errno = ENOMEM;
            return ERR;
        }
    }

    if (space_pin_depth_inc(space, buffer, pageAmount) == ERR)
    {
        errno = ENOMEM;
        return ERR;
    }

    return 0;
}

uint64_t space_pin_terminated(space_t* space, const void* address, const void* terminator, size_t objectSize,
    size_t maxCount, stack_pointer_t* userStack)
{
    if (space == NULL || address == NULL || terminator == NULL || objectSize == 0 || maxCount == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    size_t terminatorMatchedBytes = 0;
    uintptr_t current = (uintptr_t)address;
    uintptr_t end = (uintptr_t)address + (maxCount * objectSize);
    if (end < (uintptr_t)address)
    {
        errno = EOVERFLOW;
        return ERR;
    }

    LOCK_SCOPE(&space->lock);

    uint64_t pinnedPages = 0;
    while (current < end)
    {
        if (!page_table_is_mapped(&space->pageTable, (void*)current, 1))
        {
            if (userStack == NULL || !stack_pointer_is_in_stack(userStack, current, 1))
            {
                errno = EFAULT;
                goto error;
            }

            if (space_populate_user_region(space, (void*)ROUND_DOWN(current, PAGE_SIZE), 1) == ERR)
            {
                errno = ENOMEM;
                goto error;
            }
        }

        if (space_pin_depth_inc(space, (void*)current, 1) == ERR)
        {
            errno = ENOMEM;
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
                    return scanAddr - (uintptr_t)address + 1;
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
                        return scanAddr - (uintptr_t)address + 1;
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

error:
    space_pin_depth_dec(space, address, pinnedPages);
    return ERR;
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

uint64_t space_check_access(space_t* space, const void* addr, size_t length)
{
    if (space == NULL || (addr == NULL && length != 0))
    {
        errno = EINVAL;
        return ERR;
    }

    if (length == 0)
    {
        return 0;
    }

    uintptr_t addrOverflow = (uintptr_t)addr + length;
    if (addrOverflow < (uintptr_t)addr)
    {
        errno = EOVERFLOW;
        return ERR;
    }

    if ((uintptr_t)addr < space->startAddress || addrOverflow > space->endAddress)
    {
        errno = EFAULT;
        return ERR;
    }

    return 0;
}

static void* space_find_free_region(space_t* space, uint64_t pageAmount, uint64_t alignment)
{
    void* addr;
    if (page_table_find_unmapped_region(&space->pageTable, (void*)space->freeAddress, (void*)space->endAddress,
            pageAmount, alignment, &addr) != ERR)
    {
        space->freeAddress = (uintptr_t)addr + pageAmount * PAGE_SIZE;
        assert(page_table_is_unmapped(&space->pageTable, addr, pageAmount));
        return addr;
    }

    if (page_table_find_unmapped_region(&space->pageTable, (void*)space->startAddress, (void*)space->freeAddress,
            pageAmount, alignment, &addr) != ERR)
    {
        assert(page_table_is_unmapped(&space->pageTable, addr, pageAmount));
        return addr;
    }

    return NULL;
}

uint64_t space_mapping_start(space_t* space, space_mapping_t* mapping, void* virtAddr, phys_addr_t physAddr,
    size_t length, size_t alignment, pml_flags_t flags)
{
    if (space == NULL || mapping == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    uintptr_t virtOverflow = (uintptr_t)virtAddr + length;
    if (virtOverflow < (uintptr_t)virtAddr)
    {
        errno = EOVERFLOW;
        return ERR;
    }

    uintptr_t physOverflow = (uintptr_t)physAddr + length;
    if (physAddr != PHYS_ADDR_INVALID && physOverflow < (uintptr_t)physAddr)
    {
        errno = EOVERFLOW;
        return ERR;
    }

    if (flags & PML_USER)
    {
        if (virtAddr != NULL &&
            ((uintptr_t)virtAddr + length > VMM_USER_SPACE_MAX || (uintptr_t)virtAddr < VMM_USER_SPACE_MIN))
        {
            errno = EFAULT;
            return ERR;
        }
    }

    stack_pointer_poke(1000); // 1000 bytes should be enough.

    lock_acquire(&space->lock);

    uint64_t pageAmount;
    if (virtAddr == NULL)
    {
        pageAmount = BYTES_TO_PAGES(length);
        virtAddr = space_find_free_region(space, pageAmount, alignment);
        if (virtAddr == NULL)
        {
            lock_release(&space->lock);
            errno = ENOMEM;
            return ERR;
        }
    }
    else
    {
        space_align_region(&virtAddr, &length);
        pageAmount = BYTES_TO_PAGES(length);

        if ((uintptr_t)virtAddr % alignment != 0)
        {
            lock_release(&space->lock);
            errno = EINVAL;
            return ERR;
        }
    }

    mapping->virtAddr = virtAddr;
    if (physAddr != PHYS_ADDR_INVALID)
    {
        mapping->physAddr = PML_ENSURE_LOWER_HALF(ROUND_DOWN(physAddr, PAGE_SIZE));
    }
    else
    {
        mapping->physAddr = PHYS_ADDR_INVALID;
    }

    mapping->flags = flags;
    mapping->pageAmount = pageAmount;
    return 0; // We return with the lock still acquired.
}

pml_callback_id_t space_alloc_callback(space_t* space, size_t pageAmount, space_callback_func_t func, void* data)
{
    if (space == NULL)
    {
        return PML_MAX_CALLBACK;
    }

    pml_callback_id_t callbackId = bitmap_find_first_clear(&space->callbackBitmap, 0, PML_MAX_CALLBACK);
    if (callbackId == PML_MAX_CALLBACK)
    {
        return PML_MAX_CALLBACK;
    }

    if (callbackId >= space->callbacksLength)
    {
        space_callback_t* newCallbacks = malloc(sizeof(space_callback_t) * (callbackId + 1));
        if (newCallbacks == NULL)
        {
            return PML_MAX_CALLBACK;
        }

        if (space->callbacks != NULL)
        {
            memcpy(newCallbacks, space->callbacks, sizeof(space_callback_t) * space->callbacksLength);
            free(space->callbacks);
        }
        memset(&newCallbacks[space->callbacksLength], 0,
            sizeof(space_callback_t) * (callbackId + 1 - space->callbacksLength));

        space->callbacks = newCallbacks;
        space->callbacksLength = callbackId + 1;
    }

    bitmap_set(&space->callbackBitmap, callbackId);
    space_callback_t* callback = &space->callbacks[callbackId];
    callback->func = func;
    callback->data = data;
    callback->pageAmount = pageAmount;
    return callbackId;
}

void space_free_callback(space_t* space, pml_callback_id_t callbackId)
{
    bitmap_clear(&space->callbackBitmap, callbackId);
}

static void space_update_free_address(space_t* space, uintptr_t virtAddr, uint64_t pageAmount)
{
    if (space == NULL)
    {
        return;
    }

    if (virtAddr <= space->freeAddress && space->freeAddress < virtAddr + pageAmount * PAGE_SIZE)
    {
        space->freeAddress = virtAddr + pageAmount * PAGE_SIZE;
    }
}

void* space_mapping_end(space_t* space, space_mapping_t* mapping, errno_t err)
{
    if (space == NULL || mapping == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (err != EOK)
    {
        lock_release(&space->lock); // Release the lock from space_mapping_start.
        errno = err;
        return NULL;
    }

    space_update_free_address(space, (uintptr_t)mapping->virtAddr, mapping->pageAmount);
    lock_release(&space->lock); // Release the lock from space_mapping_start.
    return mapping->virtAddr;
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
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&space->lock);
    return page_table_count_pages_with_flags(&space->pageTable, (void*)VMM_USER_SPACE_MIN,
        BYTES_TO_PAGES(VMM_USER_SPACE_MAX - VMM_USER_SPACE_MIN), PML_PRESENT | PML_USER | PML_OWNED);
}

phys_addr_t space_virt_to_phys(space_t* space, const void* virtAddr)
{
    if (space == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    phys_addr_t physAddr;
    LOCK_SCOPE(&space->lock);
    if (page_table_get_phys_addr(&space->pageTable, (void*)virtAddr, &physAddr) == ERR)
    {
        errno = EFAULT;
        return ERR;
    }

    return physAddr;
}