#include "vmm.h"

#include "utils.h"
#include "heap.h"
#include "lock.h"
#include "pmm.h"
#include "sched.h"
#include "regs.h"
#include "debug.h"

static PageTable* kernelPageTable;

static uint64_t vmm_prot_to_flags(uint8_t prot)
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

void space_init(Space* space)
{
    space->pageTable = page_table_new();
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
        space->pageTable->entries[i] = 0;
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

void* vmm_kernel_map(void* virtualAddress, void* physicalAddress, uint64_t length, uint64_t flags)
{
    if (virtualAddress == NULL)
    {
        virtualAddress = VMM_LOWER_TO_HIGHER(physicalAddress);
    }

    page_table_map(kernelPageTable, virtualAddress, physicalAddress, SIZE_IN_PAGES(length), flags | VMM_KERNEL_PAGE_FLAGS);

    return virtualAddress;
}

void* vmm_allocate(void* virtualAddress, uint64_t length, uint8_t prot)
{
    if (virtualAddress == NULL)
    {
        return NULLPTR(EFAULT);
    }

    Space* space = &sched_process()->space;

    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    { 
        return NULLPTR(EACCES);
    }
    flags |= PAGE_FLAG_OWNED;

    virtualAddress = (void*)ROUND_DOWN(virtualAddress, PAGE_SIZE);
    LOCK_GUARD(&space->lock);

    if (page_table_mapped(space->pageTable, virtualAddress, SIZE_IN_PAGES(length)))
    {
        return NULLPTR(EEXIST);
    }

    for (uint64_t i = 0; i < SIZE_IN_PAGES(length); i++)
    {
        void* address = (void*)((uint64_t)virtualAddress + i * PAGE_SIZE);
        page_table_map(space->pageTable, address, pmm_allocate(), 1, flags);
    }

    return virtualAddress;
}

void* vmm_map(void* virtualAddress, void* physicalAddress, uint64_t length, uint8_t prot)
{
    if (virtualAddress == NULL || physicalAddress == NULL)
    {
        return NULLPTR(EFAULT);
    }

    Space* space = &sched_process()->space;

    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    { 
        return NULLPTR(EACCES);
    }

    virtualAddress = (void*)ROUND_DOWN(virtualAddress, PAGE_SIZE);
    physicalAddress = (void*)ROUND_DOWN(physicalAddress, PAGE_SIZE);
    LOCK_GUARD(&space->lock);

    if (page_table_mapped(space->pageTable, virtualAddress, SIZE_IN_PAGES(length)))
    {
        return NULLPTR(EEXIST);
    }

    page_table_map(space->pageTable, virtualAddress, physicalAddress, SIZE_IN_PAGES(length), flags);

    return virtualAddress;
}

uint64_t vmm_unmap(void* virtualAddress, uint64_t length)
{    
    Space* space = &sched_process()->space;

    virtualAddress = (void*)ROUND_DOWN(virtualAddress, PAGE_SIZE);
    LOCK_GUARD(&space->lock);

    if (!page_table_mapped(space->pageTable, virtualAddress, SIZE_IN_PAGES(length)))
    {
        return ERROR(EFAULT);
    }

    page_table_unmap(space->pageTable, virtualAddress, SIZE_IN_PAGES(length));

    return 0;
}

uint64_t vmm_protect(void* virtualAddress, uint64_t length, uint8_t prot)
{
    Space* space = &sched_process()->space;

    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    { 
        return ERROR(EACCES);
    }
    flags |= PAGE_FLAG_USER;

    virtualAddress = (void*)ROUND_DOWN(virtualAddress, PAGE_SIZE);
    LOCK_GUARD(&space->lock);

    if (!page_table_mapped(space->pageTable, virtualAddress, SIZE_IN_PAGES(length)))
    {
        return ERROR(EFAULT);
    }

    page_table_change_flags(space->pageTable, virtualAddress, SIZE_IN_PAGES(length), flags);

    return 0;
}

void* vmm_virt_to_phys(const void* virtualAddress)
{
    Space* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    virtualAddress = (void*)ROUND_DOWN(virtualAddress, PAGE_SIZE);

    if (!page_table_mapped(space->pageTable, virtualAddress, 1))
    {
        return NULL;
    }

    return page_table_physical_address(space->pageTable, virtualAddress);
}