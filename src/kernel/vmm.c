#include "vmm.h"

#include "debug.h"
#include "lock.h"
#include "pmm.h"
#include "regs.h"
#include "sched.h"
#include "utils.h"

#include <stdlib.h>

static PageTable* kernelPageTable;

static List blocks;

static void vmm_align_region(void** virtAddr, uint64_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*virtAddr, PAGE_SIZE);
    *length += ((uint64_t)*virtAddr - (uint64_t)aligned);
    *virtAddr = aligned;
}

static uint64_t vmm_prot_to_flags(prot_t prot)
{
    if (!(prot & PROT_READ))
    {
        return ERR;
    }

    return (prot & PROT_WRITE ? PAGE_FLAG_WRITE : 0) | PAGE_FLAG_USER;
}

static void vmm_load_memory_map(EfiMemoryMap* memoryMap)
{
    kernelPageTable = page_table_new();

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        page_table_map(kernelPageTable, desc->virtualStart, desc->physicalStart, desc->amountOfPages,
            PAGE_FLAG_WRITE | VMM_KERNEL_PAGE_FLAGS);
    }

    page_table_load(kernelPageTable);
}

static void vmm_deallocate_boot_page_table(EfiMemoryMap* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (desc->type == EFI_MEMORY_TYPE_PAGE_TABLE)
        {
            pmm_free_pages(desc->physicalStart, desc->amountOfPages);
        }
    }
}

static void* space_find_free_region(Space* space, uint64_t length)
{
    uint64_t pageAmount = SIZE_IN_PAGES(length);

    for (uintptr_t addr = space->freeAddress; addr < ROUND_DOWN(UINT64_MAX, 0x1000); addr += pageAmount * PAGE_SIZE)
    {
        if (!page_table_mapped(space->pageTable, (void*)addr, pageAmount))
        {
            space->freeAddress = addr + pageAmount * PAGE_SIZE;
            return (void*)addr;
        }
    }

    debug_panic("Address space filled, you must have ran this on a super computer... dont do that.");
}

void space_init(Space* space)
{
    space->pageTable = page_table_new();
    space->freeAddress = 0x400000;
    lock_init(&space->lock);

    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pageTable->entries[i] = kernelPageTable->entries[i];
    }
}

void space_cleanup(Space* space)
{
    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pageTable->entries[i] = (PageEntry){0};
    }

    page_table_free(space->pageTable);
}

void space_load(Space* space)
{
    if (space == NULL)
    {
        page_table_load(kernelPageTable);
    }
    else
    {
        page_table_load(space->pageTable);
    }
}

void vmm_init(EfiMemoryMap* memoryMap)
{
    vmm_load_memory_map(memoryMap);
    vmm_deallocate_boot_page_table(memoryMap);
}

void* vmm_kernel_map(void* virtAddr, void* physAddr, uint64_t length)
{
    if (virtAddr == NULL)
    {
        virtAddr = VMM_LOWER_TO_HIGHER(physAddr);
    }

    page_table_map(kernelPageTable, virtAddr, physAddr, SIZE_IN_PAGES(length), PAGE_FLAG_WRITE | VMM_KERNEL_PAGE_FLAGS);

    return virtAddr;
}

void* vmm_alloc(void* virtAddr, uint64_t length, prot_t prot)
{
    Space* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    if (length == 0)
    {
        return NULLPTR(EINVAL);
    }

    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    {
        return NULLPTR(EACCES);
    }
    flags |= PAGE_FLAG_OWNED;

    if (virtAddr == NULL)
    {
        virtAddr = space_find_free_region(space, length);
    }

    vmm_align_region(&virtAddr, &length);

    if (page_table_mapped(space->pageTable, virtAddr, SIZE_IN_PAGES(length)))
    {
        return NULLPTR(EEXIST);
    }

    for (uint64_t i = 0; i < SIZE_IN_PAGES(length); i++)
    {
        void* address = (void*)((uint64_t)virtAddr + i * PAGE_SIZE);
        page_table_map(space->pageTable, address, pmm_alloc(), 1, flags);
    }

    return virtAddr;
}

void* vmm_map(void* virtAddr, void* physAddr, uint64_t length, prot_t prot)
{
    Space* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    if (physAddr == NULL)
    {
        return NULLPTR(EFAULT);
    }

    if (length == 0)
    {
        return NULLPTR(EINVAL);
    }

    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    {
        return NULLPTR(EACCES);
    }

    if (virtAddr == NULL)
    {
        virtAddr = space_find_free_region(space, length);
    }

    physAddr = (void*)ROUND_DOWN(physAddr, PAGE_SIZE);
    vmm_align_region(&virtAddr, &length);

    if (page_table_mapped(space->pageTable, virtAddr, SIZE_IN_PAGES(length)))
    {
        return NULLPTR(EEXIST);
    }

    page_table_map(space->pageTable, virtAddr, physAddr, SIZE_IN_PAGES(length), flags);

    return virtAddr;
}

uint64_t vmm_unmap(void* virtAddr, uint64_t length)
{
    vmm_align_region(&virtAddr, &length);

    Space* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    if (!page_table_mapped(space->pageTable, virtAddr, SIZE_IN_PAGES(length)))
    {
        return ERROR(EFAULT);
    }

    page_table_unmap(space->pageTable, virtAddr, SIZE_IN_PAGES(length));

    return 0;
}

uint64_t vmm_protect(void* virtAddr, uint64_t length, prot_t prot)
{
    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    {
        return ERROR(EACCES);
    }

    vmm_align_region(&virtAddr, &length);

    Space* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    if (!page_table_mapped(space->pageTable, virtAddr, SIZE_IN_PAGES(length)))
    {
        return ERROR(EFAULT);
    }

    page_table_change_flags(space->pageTable, virtAddr, SIZE_IN_PAGES(length), flags);

    return 0;
}

bool vmm_mapped(const void* virtAddr, uint64_t length)
{
    vmm_align_region((void**)&virtAddr, &length);

    Space* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    return page_table_mapped(space->pageTable, virtAddr, SIZE_IN_PAGES(length));
}