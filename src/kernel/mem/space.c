#include "space.h"

#include "mem/space.h"
#include "pmm.h"
#include "vmm.h"

#include <common/paging.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/math.h>
#include <sys/proc.h>

static void* space_pmm_bitmap_alloc(void)
{
    return pmm_alloc_bitmap(1, UINT32_MAX, 0);
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
        if (page_table_init(&space->pageTable, space_pmm_bitmap_alloc, pmm_free) == ERR)
        {
            errno = ENOMEM;
            return ERR;
        }
        // We only use the specific pmm allocator for the page table itself, not for mappings.
        space->pageTable.allocPage = pmm_alloc;
    }
    else
    {
        if (page_table_init(&space->pageTable, pmm_alloc, pmm_free) == ERR)
        {
            errno = ENOMEM;
            return ERR;
        }
    }

    space->startAddress = startAddress;
    space->endAddress = endAddress;
    space->freeAddress = startAddress;
    space->flags = flags;
    memset(space->callbacks, 0, sizeof(space_callback_t) * PML_MAX_CALLBACK);
    bitmap_init(&space->callbackBitmap, space->bitmapBuffer, PML_MAX_CALLBACK);
    memset(space->bitmapBuffer, 0, BITMAP_BITS_TO_BYTES(PML_MAX_CALLBACK));
    wait_queue_init(&space->pinWaitQueue);
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

    wait_queue_deinit(&space->pinWaitQueue);
    page_table_deinit(&space->pageTable);
}

void space_load(space_t* space)
{
    if (space == NULL)
    {
        return;
    }

    page_table_load(&space->pageTable);
}

static void space_align_region(void** virtAddr, uint64_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*virtAddr, PAGE_SIZE);
    *length += ((uint64_t)*virtAddr - (uint64_t)aligned);
    *virtAddr = aligned;
}

uint64_t space_pin(space_t* space, const void* buffer, uint64_t length)
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
        errno = EFAULT;
        return ERR;
    }

    if (WAIT_BLOCK_LOCK(&space->pinWaitQueue, &space->lock,
            !page_table_is_pinned(&space->pageTable, buffer, pageAmount)) == ERR)
    {
        return ERR;
    }

    if (!page_table_is_mapped(&space->pageTable, buffer, pageAmount)) // Important recheck
    {
        errno = EFAULT;
        return ERR;
    }

    if (page_table_pin(&space->pageTable, buffer, pageAmount) == ERR)
    {
        errno = EFAULT;
        return ERR;
    }

    return 0;
}

uint64_t space_pin_terminated(space_t* space, const void* address, const void* terminator, uint8_t objectSize,
    uint64_t maxCount)
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
            errno = EFAULT;
            goto error;
        }

        if (WAIT_BLOCK_LOCK(&space->pinWaitQueue, &space->lock,
                !page_table_is_pinned(&space->pageTable, (void*)current, 1)) == ERR)
        {
            return ERR;
        }

        if (!page_table_is_mapped(&space->pageTable, (void*)current, 1)) // Important recheck
        {
            errno = EFAULT;
            goto error;
        }

        if (page_table_pin(&space->pageTable, (void*)current, 1) == ERR)
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
    page_table_unpin(&space->pageTable, address, pinnedPages);
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

    page_table_unpin(&space->pageTable, address, pageAmount);

    wait_unblock(&space->pinWaitQueue, WAIT_ALL, EOK);
}

uint64_t space_safe_copy_from(space_t* space, void* dest, const void* src, uint64_t length)
{
    if (space == NULL || dest == NULL || src == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space_pin(space, src, length) == ERR)
    {
        return ERR;
    }

    memcpy(dest, src, length);
    space_unpin(space, src, length);
    return 0;
}

uint64_t space_safe_copy_to(space_t* space, void* dest, const void* src, uint64_t length)
{
    if (space == NULL || dest == NULL || src == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space_pin(space, dest, length) == ERR)
    {
        return ERR;
    }

    memcpy(dest, src, length);
    space_unpin(space, dest, length);
    return 0;
}

uint64_t space_safe_pathname_init(space_t* space, pathname_t* pathname, const char* path)
{
    if (space == NULL || pathname == NULL || path == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    char terminator = '\0';
    uint64_t pathLength = space_pin_terminated(space, path, &terminator, sizeof(char), MAX_PATH);
    if (pathLength == ERR)
    {
        return ERR;
    }

    char copy[MAX_PATH];
    memcpy(copy, path, pathLength);
    copy[pathLength] = '\0';
    space_unpin(space, path, pathLength);

    if (pathname_init(pathname, copy) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t space_safe_atomic_uint64_t_load(space_t* space, atomic_uint64_t* obj, uint64_t* outValue)
{
    if (space == NULL || obj == NULL || outValue == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space_pin(space, obj, sizeof(atomic_uint64_t)) == ERR)
    {
        return ERR;
    }

    *outValue = atomic_load(obj);
    space_unpin(space, obj, sizeof(atomic_uint64_t));
    return 0;
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
    uintptr_t addr = space->freeAddress;
    while (addr < space->endAddress)
    {
        void* firstMappedPage;
        if (page_table_find_first_mapped_page(&space->pageTable, (void*)addr, (void*)(addr + pageAmount * PAGE_SIZE),
                &firstMappedPage) != ERR)
        {
            addr = (uintptr_t)firstMappedPage + PAGE_SIZE;
            continue;
        }

        space->freeAddress = addr + pageAmount * PAGE_SIZE;
        return (void*)addr;
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
