#include "vmm.h"

#include "common/boot_info.h"
#include "debug.h"
#include "lock.h"
#include "pmm.h"
#include "regs.h"
#include "sched.h"
#include "utils.h"
#include "log.h"

#include <stdlib.h>

static pml_t* kernelPageTable;

static list_t blocks;

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

    return (prot & PROT_WRITE ? PAGE_WRITE : 0) | PAGE_USER;
}

static void vmm_load_memory_map(efi_mem_map_t* memoryMap)
{
    kernelPageTable = pml_new();

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        pml_map(kernelPageTable, desc->virtualStart, desc->physicalStart, desc->amountOfPages, PAGE_WRITE | VMM_KERNEL_PAGES);
    }

    pml_load(kernelPageTable);
}

static void* space_find_free_region(space_t* space, uint64_t length)
{
    uint64_t pageAmount = SIZE_IN_PAGES(length);

    for (uintptr_t addr = space->freeAddress; addr < ROUND_DOWN(UINT64_MAX, 0x1000); addr += pageAmount * PAGE_SIZE)
    {
        if (!pml_mapped(space->pml, (void*)addr, pageAmount))
        {
            space->freeAddress = addr + pageAmount * PAGE_SIZE;
            return (void*)addr;
        }
    }

    debug_panic("Address space filled, you must have ran this on a super computer... dont do that.");
}

void space_init(space_t* space)
{
    space->pml = pml_new();
    space->freeAddress = 0x400000;
    lock_init(&space->lock);

    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pml->entries[i] = kernelPageTable->entries[i];
    }
}

void space_cleanup(space_t* space)
{
    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pml->entries[i] = (pml_entry_t){0};
    }

    pml_free(space->pml);
}

void space_load(space_t* space)
{
    if (space == NULL)
    {
        pml_load(kernelPageTable);
    }
    else
    {
        pml_load(space->pml);
    }
}

void vmm_init(efi_mem_map_t* memoryMap, gop_buffer_t* gopBuffer)
{
    vmm_load_memory_map(memoryMap);
    log_print("Kernel PML loaded %a", kernelPageTable);

    pmm_free_type(EFI_MEM_BOOT_PML);

    gopBuffer->base = vmm_kernel_map(NULL, gopBuffer->base, gopBuffer->size);
}

void* vmm_kernel_map(void* virtAddr, void* physAddr, uint64_t length)
{
    if (virtAddr == NULL)
    {
        virtAddr = VMM_LOWER_TO_HIGHER(physAddr);
    }

    pml_map(kernelPageTable, virtAddr, physAddr, SIZE_IN_PAGES(length), PAGE_WRITE | VMM_KERNEL_PAGES);

    return virtAddr;
}

void* vmm_alloc(void* virtAddr, uint64_t length, prot_t prot)
{
    space_t* space = &sched_process()->space;
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
    flags |= PAGE_OWNED;

    if (virtAddr == NULL)
    {
        virtAddr = space_find_free_region(space, length);
    }

    vmm_align_region(&virtAddr, &length);

    if (pml_mapped(space->pml, virtAddr, SIZE_IN_PAGES(length)))
    {
        return NULLPTR(EEXIST);
    }

    for (uint64_t i = 0; i < SIZE_IN_PAGES(length); i++)
    {
        void* address = (void*)((uint64_t)virtAddr + i * PAGE_SIZE);
        pml_map(space->pml, address, pmm_alloc(), 1, flags);
    }

    return virtAddr;
}

void* vmm_map(void* virtAddr, void* physAddr, uint64_t length, prot_t prot)
{
    space_t* space = &sched_process()->space;
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

    if (pml_mapped(space->pml, virtAddr, SIZE_IN_PAGES(length)))
    {
        return NULLPTR(EEXIST);
    }

    pml_map(space->pml, virtAddr, physAddr, SIZE_IN_PAGES(length), flags);

    return virtAddr;
}

uint64_t vmm_unmap(void* virtAddr, uint64_t length)
{
    vmm_align_region(&virtAddr, &length);

    space_t* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    if (!pml_mapped(space->pml, virtAddr, SIZE_IN_PAGES(length)))
    {
        return ERROR(EFAULT);
    }

    pml_unmap(space->pml, virtAddr, SIZE_IN_PAGES(length));

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

    space_t* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    if (!pml_mapped(space->pml, virtAddr, SIZE_IN_PAGES(length)))
    {
        return ERROR(EFAULT);
    }

    pml_change_flags(space->pml, virtAddr, SIZE_IN_PAGES(length), flags);

    return 0;
}

bool vmm_mapped(const void* virtAddr, uint64_t length)
{
    vmm_align_region((void**)&virtAddr, &length);

    space_t* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    return pml_mapped(space->pml, virtAddr, SIZE_IN_PAGES(length));
}
