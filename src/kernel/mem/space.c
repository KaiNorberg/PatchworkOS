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

#ifndef NDEBUG
extern uint64_t _kernelEnd;
#endif

static void* space_pmm_bitmap_alloc(void)
{
    return pmm_alloc_bitmap(1, UINT32_MAX, 0);
}

uint64_t space_init(space_t* space, space_t* parent, uintptr_t startAddress, uintptr_t endAddress, space_flags_t flags)
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

    space->parent = parent;
    space->endAddress = endAddress;
    space->freeAddress = startAddress;
    memset(space->callbacks, 0, sizeof(space_callback_t) * PML_MAX_CALLBACK);
    bitmap_init(&space->callbackBitmap, space->bitmapBuffer, PML_MAX_CALLBACK);
    memset(space->bitmapBuffer, 0, BITMAP_BITS_TO_BYTES(PML_MAX_CALLBACK));
    rwmutex_init(&space->mutex);

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

    for (uint64_t i = 0; i < PML_ENTRY_AMOUNT; i++)
    {
        // Remove inherited mappings so they don't get freed.
        if (space->pageTable.pml4->entries[i].inherit)
        {
            space->pageTable.pml4->entries[i].raw = 0;
        }
    }

    page_table_deinit(&space->pageTable);
}

void space_load(space_t* space)
{
    if (space == NULL)
    {
        return;
    }

    if (space->parent == NULL)
    {
        page_table_load(&space->pageTable);
        return;
    }

    for (uint64_t i = 0; i < PML_ENTRY_AMOUNT; i++)
    {
        if (space->parent->pageTable.pml4->entries[i].inherit)
        {
            if (!space->pageTable.pml4->entries[i].inherit && space->pageTable.pml4->entries[i].present)
            {
                panic(NULL, "Address space inheritance conflict at PML4 entry %d", i);
            }

            space->pageTable.pml4->entries[i] = space->parent->pageTable.pml4->entries[i];
        }
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

        assert(space != vmm_get_kernel_space() || addr >= (uint64_t)&_kernelEnd);

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

    if (flags & PML_USER)
    {
        if (virtAddr != NULL && ((uintptr_t)virtAddr >= PML_LOWER_HALF_END || (uintptr_t)virtAddr < PML_LOWER_HALF_START))
        {
            errno = EFAULT;
            return ERR;
        }

        if (physAddr != NULL && ((uintptr_t)physAddr >= PML_LOWER_HALF_END || (uintptr_t)physAddr < PML_LOWER_HALF_START))
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
    if (mapping->physAddr != NULL)
    {
        mapping->physAddr = PML_ENSURE_LOWER_HALF((void*)ROUND_DOWN(physAddr, PAGE_SIZE));
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
