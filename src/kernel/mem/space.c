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

uint64_t space_init(space_t* space)
{
    if (space == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (page_table_init(&space->pageTable, pmm_alloc, pmm_free) == ERR)
    {
        errno = ENOMEM;
        return ERR;
    }

    space->freeAddress = PML_LOWER_HALF_START;
    memset(space->callbacks, 0, sizeof(space_callback_t) * PML_MAX_CALLBACK);
    memset(space->bitmapBuffer, 0, BITMAP_BITS_TO_BYTES(PML_MAX_CALLBACK));
    bitmap_init(&space->callbackBitmap, space->bitmapBuffer, PML_MAX_CALLBACK);
    rwmutex_init(&space->mutex);

    page_table_t* kernelPageTable = vmm_kernel_pml();
    for (uint64_t i = PML_ENTRY_AMOUNT / 2; i < PML_ENTRY_AMOUNT; i++)
    {
        space->pageTable.pml4->entries[i] = kernelPageTable->pml4->entries[i];
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

    for (uint64_t i = PML_ENTRY_AMOUNT / 2; i < PML_ENTRY_AMOUNT; i++)
    {
        space->pageTable.pml4->entries[i].raw = 0;
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

static uint64_t space_prot_to_flags(prot_t prot, pml_flags_t* flags)
{
    if (!(prot & PROT_READ))
    {
        return ERR;
    }
    *flags = (prot & PROT_WRITE ? PML_WRITE : 0) | PML_USER;
    return 0;
}

static void* space_find_free_region(space_t* space, uint64_t pageAmount)
{
    uintptr_t addr = space->freeAddress;
    while (addr < PML_LOWER_HALF_END)
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
    prot_t prot)
{
    if (space == NULL || mapping == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space_prot_to_flags(prot, &mapping->flags) == ERR)
    {
        return ERR;
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
        errno = err;
        return NULL;
    }

    space_update_free_address(space, (uintptr_t)mapping->virtAddr, mapping->pageAmount);
    rwmutex_write_release(&space->mutex); // Release the mutex from space_mapping_start.
    return mapping->virtAddr;
}

uint64_t space_unmap(space_t* space, void* virtAddr, uint64_t length)
{
    if (space == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    RWMUTEX_WRITE_SCOPE(&space->mutex);

    if (page_table_is_unmapped(&space->pageTable, virtAddr, pageAmount))
    {
        errno = EFAULT;
        return ERR;
    }

    uint64_t callbacks[PML_MAX_CALLBACK] = {
        0}; // Stores the amount of pages that have each callback id within the region.
    page_table_collect_callbacks(&space->pageTable, virtAddr, pageAmount, callbacks);

    page_table_unmap(&space->pageTable, virtAddr, pageAmount);

    uint64_t index;
    BITMAP_FOR_EACH_SET(&index, &space->callbackBitmap)
    {
        space_callback_t* callback = &space->callbacks[index];
        assert(callback->pageAmount >= callbacks[index]);

        callback->pageAmount -= callbacks[index];
        if (callback->pageAmount == 0)
        {
            space->callbacks[index].func(space->callbacks[index].private);
            space_free_callback(space, index);
        }
    }

    return 0;
}

SYSCALL_DEFINE(SYS_MUNMAP, uint64_t, void* address, uint64_t length)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!syscall_is_pointer_valid(address, length))
    {
        errno = EFAULT;
        return ERR;
    }

    return space_unmap(space, address, length);
}

uint64_t space_protect(space_t* space, void* virtAddr, uint64_t length, prot_t prot)
{
    if (space == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    pml_flags_t flags;
    if (space_prot_to_flags(prot, &flags) == ERR)
    {
        errno = EINVAL;
        return ERR;
    }

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    RWMUTEX_READ_SCOPE(&space->mutex);

    if (page_table_is_unmapped(&space->pageTable, virtAddr, pageAmount))
    {
        errno = EFAULT;
        return ERR;
    }

    uint64_t result = page_table_set_flags(&space->pageTable, virtAddr, pageAmount, flags);
    if (result == ERR)
    {
        errno = ENOENT;
        return ERR;
    }

    return 0;
}

SYSCALL_DEFINE(SYS_MPROTECT, uint64_t, void* address, uint64_t length, prot_t prot)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!syscall_is_pointer_valid(address, length))
    {
        errno = EFAULT;
        return ERR;
    }

    return space_protect(space, address, length, prot);
}

bool space_is_mapped(space_t* space, const void* virtAddr, uint64_t length)
{
    vmm_align_region((void**)&virtAddr, &length);
    RWMUTEX_READ_SCOPE(&space->mutex);
    return page_table_is_mapped(&space->pageTable, virtAddr, BYTES_TO_PAGES(length));
}
