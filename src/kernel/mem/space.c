#include "space.h"

#include "cpu/syscalls.h"
#include "mem/space.h"
#include "pmm.h"
#include "proc/process.h"
#include "sync/rwmutex.h"
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

    space->endAddress = endAddress;
    space->freeAddress = startAddress;
    space->flags = flags;
    memset(space->callbacks, 0, sizeof(space_callback_t) * PML_MAX_CALLBACK);
    bitmap_init(&space->callbackBitmap, space->bitmapBuffer, PML_MAX_CALLBACK);
    memset(space->bitmapBuffer, 0, BITMAP_BITS_TO_BYTES(PML_MAX_CALLBACK));
    rwmutex_init(&space->mutex);

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

    rwmutex_deinit(&space->mutex);

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
    if (space == NULL || mapping == NULL || length == 0 || !(flags & PML_PRESENT))
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

    rwmutex_write_acquire(&space->mutex);

    uint64_t pageAmount;
    if (virtAddr == NULL)
    {
        pageAmount = BYTES_TO_PAGES(length);
        virtAddr = space_find_free_region(space, pageAmount);
        if (virtAddr == NULL)
        {
            rwmutex_write_release(&space->mutex);
            errno = ENOMEM;
            return ERR;
        }
    }
    else
    {
        vmm_align_region(&virtAddr, &length);
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

    mapping->pageAmount = pageAmount;
    mapping->flags = flags;
    return 0; // We return with the mutex still acquired.
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

    if (err != 0)
    {
        rwmutex_write_release(&space->mutex); // Release the mutex from space_mapping_start.
        errno = err;
        return NULL;
    }

    space_update_free_address(space, (uintptr_t)mapping->virtAddr, mapping->pageAmount);
    rwmutex_write_release(&space->mutex); // Release the mutex from space_mapping_start.
    return mapping->virtAddr;
}

bool space_is_mapped(space_t* space, const void* virtAddr, uint64_t length)
{
    vmm_align_region((void**)&virtAddr, &length);
    RWMUTEX_READ_SCOPE(&space->mutex);
    return page_table_is_mapped(&space->pageTable, virtAddr, BYTES_TO_PAGES(length));
}
