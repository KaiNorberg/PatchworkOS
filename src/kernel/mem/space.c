#include <kernel/mem/space.h>

#include <kernel/cpu/cpu.h>
#include <kernel/log/panic.h>
#include <kernel/mem/paging.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/space.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sys_time.h>
#include <kernel/utils/map.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>
#include <sys/proc.h>

static uint64_t space_pmm_bitmap_alloc_pages(void** pages, uint64_t pageAmount)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        void* page = pmm_alloc_bitmap(1, UINT32_MAX, 0);
        if (page == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                pmm_free(pages[j]);
            }
            return ERR;
        }
        pages[i] = page;
    }
    return 0;
}

static inline void space_map_kernel_space_region(space_t* space, uintptr_t start, uintptr_t end)
{
    space_t* kernelSpace = vmm_get_kernel_space();
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
    bitmap_init(&space->callbackBitmap, space->bitmapBuffer, PML_MAX_CALLBACK);
    memset(space->bitmapBuffer, 0, BITMAP_BITS_TO_BYTES(PML_MAX_CALLBACK));
    list_init(&space->cpus);
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

    if (!list_is_empty(&space->cpus))
    {
        panic(NULL, "Attempted to free address space still in use by CPUs");
    }

    uint64_t index;
    BITMAP_FOR_EACH_SET(&index, &space->callbackBitmap)
    {
        space->callbacks[index].func(space->callbacks[index].private);
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

void space_load(space_t* space)
{
    if (space == NULL)
    {
        return;
    }

    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    cpu_t* self = cpu_get_unsafe();
    assert(self != NULL);

    assert(self->vmm.currentSpace != NULL);
    if (space == self->vmm.currentSpace)
    {
        return;
    }

    space_t* oldSpace = self->vmm.currentSpace;
    self->vmm.currentSpace = NULL;

    lock_acquire(&oldSpace->lock);
#ifndef NDEBUG
    bool found = false;
    cpu_t* cpu;
    LIST_FOR_EACH(cpu, &oldSpace->cpus, vmm.entry)
    {
        if (self == cpu)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        lock_release(&oldSpace->lock);
        panic(NULL, "CPU not found in old space's CPU list");
    }
#endif
    list_remove(&oldSpace->cpus, &self->vmm.entry);
    lock_release(&oldSpace->lock);

    lock_acquire(&space->lock);
    list_push_back(&space->cpus, &self->vmm.entry);
    lock_release(&space->lock);
    self->vmm.currentSpace = space;

    page_table_load(&space->pageTable);
}

static void space_align_region(void** virtAddr, uint64_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*virtAddr, PAGE_SIZE);
    *length += ((uint64_t)*virtAddr - (uint64_t)aligned);
    *virtAddr = aligned;
}

static uint64_t space_populate_user_region(space_t* space, const void* buffer, uint64_t pageAmount)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        uintptr_t addr = (uintptr_t)buffer + (i * PAGE_SIZE);
        if (page_table_is_mapped(&space->pageTable, (void*)addr, 1))
        {
            continue;
        }

        void* page = pmm_alloc();
        if (page == NULL)
        {
            errno = ENOMEM;
            return ERR;
        }

        if (page_table_map(&space->pageTable, (void*)addr, page, 1, PML_PRESENT | PML_USER | PML_WRITE | PML_OWNED,
                PML_CALLBACK_NONE) == ERR)
        {
            pmm_free(page);
            errno = EFAULT;
            return ERR;
        }
    }

    return 0;
}

static void space_pin_depth_dec(space_t* space, const void* address, uint64_t pageAmount)
{
    address = (void*)ROUND_DOWN((uintptr_t)address, PAGE_SIZE);

    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        uintptr_t addr = (uintptr_t)address + (i * PAGE_SIZE);
        if (page_table_traverse(&space->pageTable, &traverse, addr, PML_NONE) == ERR)
        {
            continue;
        }

        if (!traverse.entry->present || !traverse.entry->pinned)
        {
            continue;
        }

        map_key_t key = map_key_uint64(addr);
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

static inline uint64_t space_pin_depth_inc(space_t* space, const void* address, uint64_t pageAmount)
{
    address = (void*)ROUND_DOWN((uintptr_t)address, PAGE_SIZE);

    page_table_traverse_t traverse = PAGE_TABLE_TRAVERSE_CREATE;
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        uintptr_t addr = (uintptr_t)address + (i * PAGE_SIZE);
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

        map_key_t key = map_key_uint64(addr);
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
            return ERR;
        }
        map_entry_init(&newPinnedPage->mapEntry);
        newPinnedPage->pinCount = 2; // One for the page table, one for the map
        if (map_insert(&space->pinnedPages, &key, &newPinnedPage->mapEntry) == ERR)
        {
            free(newPinnedPage);
            return ERR;
        }
    }

    return 0;
}

uint64_t space_pin(space_t* space, const void* buffer, uint64_t length, stack_pointer_t* userStack)
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
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    if (!page_table_is_mapped(&space->pageTable, buffer, pageAmount))
    {
        if (userStack == NULL || !stack_pointer_is_in_stack(userStack, (uintptr_t)buffer, length))
        {
            errno = EFAULT;
            return ERR;
        }

        if (space_populate_user_region(space, buffer, pageAmount) == ERR)
        {
            return ERR;
        }
    }

    if (space_pin_depth_inc(space, buffer, pageAmount) == ERR)
    {
        errno = EFAULT;
        return ERR;
    }

    return 0;
}

uint64_t space_pin_terminated(space_t* space, const void* address, const void* terminator, uint8_t objectSize,
    uint64_t maxCount, stack_pointer_t* userStack)
{
    if (space == NULL || address == NULL || terminator == NULL || objectSize == 0 || maxCount == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&space->lock);

    uint64_t terminatorMatchedBytes = 0;
    uintptr_t current = (uintptr_t)address;
    uintptr_t end = (uintptr_t)address + (maxCount * objectSize);
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

            if (space_populate_user_region(space, (void*)current, 1) == ERR)
            {
                goto error;
            }
        }

        if (space_pin_depth_inc(space, (void*)current, 1) == ERR)
        {
            errno = EFAULT;
            goto error;
        }
        pinnedPages++;

        // Scan ONLY the currently pinned page for the terminator.
        uintptr_t scanEnd = MIN(ROUND_UP(current + 1, PAGE_SIZE), end);
        for (uintptr_t scanAddr = current; scanAddr < scanEnd; scanAddr++)
        {
            // Terminator matched bytes will wrap around to the next page
            if (*((uint8_t*)scanAddr) == ((uint8_t*)terminator)[terminatorMatchedBytes])
            {
                terminatorMatchedBytes++;
                if (terminatorMatchedBytes == objectSize)
                {
                    return scanAddr - (uintptr_t)address + 1 - objectSize;
                }
            }
            else
            {
                scanAddr += objectSize - terminatorMatchedBytes - 1; // Skip the rest of the object
                terminatorMatchedBytes = 0;
            }
        }

        current = scanEnd;
    }

error:
    space_pin_depth_dec(space, address, pinnedPages);
    return ERR;
}

void space_unpin(space_t* space, const void* address, uint64_t length)
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

uint64_t space_check_access(space_t* space, const void* addr, uint64_t length)
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

static void* space_find_free_region(space_t* space, uint64_t pageAmount)
{
    void* addr;
    if (page_table_find_unmapped_region(&space->pageTable, (void*)space->freeAddress, (void*)space->endAddress,
            pageAmount, &addr) != ERR)
    {
        space->freeAddress = (uintptr_t)addr + pageAmount * PAGE_SIZE;
        assert(page_table_is_unmapped(&space->pageTable, addr, pageAmount));
        return addr;
    }

    if (page_table_find_unmapped_region(&space->pageTable, (void*)space->startAddress, (void*)space->freeAddress,
            pageAmount, &addr) != ERR)
    {
        assert(page_table_is_unmapped(&space->pageTable, addr, pageAmount));
        return addr;
    }

    return NULL;
}

uint64_t space_mapping_start(space_t* space, space_mapping_t* mapping, void* virtAddr, void* physAddr, uint64_t length,
    pml_flags_t flags)
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
    if (physAddr != NULL && physOverflow < (uintptr_t)physAddr)
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
        virtAddr = space_find_free_region(space, pageAmount);
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
    }

    mapping->virtAddr = virtAddr;
    if (physAddr != NULL)
    {
        mapping->physAddr = (void*)PML_ENSURE_LOWER_HALF(ROUND_DOWN(physAddr, PAGE_SIZE));
    }
    else
    {
        mapping->physAddr = NULL;
    }

    mapping->flags = flags;
    mapping->pageAmount = pageAmount;
    return 0; // We return with the lock still acquired.
}

pml_callback_id_t space_alloc_callback(space_t* space, uint64_t pageAmount, space_callback_func_t func, void* private)
{
    if (space == NULL)
    {
        return PML_MAX_CALLBACK;
    }

    pml_callback_id_t callbackId = bitmap_find_first_clear(&space->callbackBitmap);
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
    callback->private = private;
    callback->pageAmount = pageAmount;
    return callbackId;
}

void space_free_callback(space_t* space, pml_callback_id_t callbackId)
{
    bitmap_clear(&space->callbackBitmap, callbackId);
}

static void space_tlb_shootdown_ipi_handler(irq_func_data_t* data)
{
    vmm_cpu_ctx_t* ctx = &data->self->vmm;
    while (true)
    {
        lock_acquire(&ctx->lock);
        if (ctx->shootdownCount == 0)
        {
            lock_release(&ctx->lock);
            break;
        }

        vmm_shootdown_t shootdown = ctx->shootdowns[ctx->shootdownCount - 1];
        ctx->shootdownCount--;
        lock_release(&ctx->lock);

        assert(shootdown.space != NULL);
        assert(shootdown.pageAmount != 0);
        assert(shootdown.virtAddr != NULL);

        tlb_invalidate(shootdown.virtAddr, shootdown.pageAmount);
        atomic_fetch_add(&shootdown.space->shootdownAcks, 1);
    }
}

void space_tlb_shootdown(space_t* space, void* virtAddr, uint64_t pageAmount)
{
    if (space == NULL)
    {
        return;
    }

    if (cpu_amount() <= 1)
    {
        return;
    }
    cpu_t* self = cpu_get_unsafe();

    uint16_t expectedAcks = 0;
    atomic_store(&space->shootdownAcks, 0);

    cpu_t* cpu;
    LIST_FOR_EACH(cpu, &space->cpus, vmm.entry)
    {
        if (cpu == self)
        {
            continue;
        }

        lock_acquire(&cpu->vmm.lock);
        if (cpu->vmm.shootdownCount >= VMM_MAX_SHOOTDOWN_REQUESTS)
        {
            lock_release(&cpu->vmm.lock);
            panic(NULL, "CPU %d shootdown buffer overflow", cpu->id);
        }

        vmm_shootdown_t* shootdown = &cpu->vmm.shootdowns[cpu->vmm.shootdownCount++];
        shootdown->space = space;
        shootdown->virtAddr = virtAddr;
        shootdown->pageAmount = pageAmount;
        lock_release(&cpu->vmm.lock);

        if (ipi_send(cpu, IPI_SINGLE, space_tlb_shootdown_ipi_handler, NULL) == ERR)
        {
            panic(NULL, "Failed to send TLB shootdown IPI to CPU %d", cpu->id);
        }
        expectedAcks++;
    }

    clock_t startTime = sys_time_uptime();
    while (atomic_load(&space->shootdownAcks) < expectedAcks)
    {
        if (sys_time_uptime() - startTime > SPACE_TLB_SHOOTDOWN_TIMEOUT)
        {
            panic(NULL, "TLB shootdown timeout in space %p for region %p - %p", space, virtAddr,
                (void*)((uintptr_t)virtAddr + pageAmount * PAGE_SIZE));
        }

        asm volatile("pause");
    }
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

bool space_is_mapped(space_t* space, const void* virtAddr, uint64_t length)
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
