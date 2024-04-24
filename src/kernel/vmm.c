#include "vmm.h"

#include "utils.h"
#include "heap.h"
#include "lock.h"
#include "pmm.h"
#include "sched.h"
#include "regs.h"
#include "debug.h"

static PageTable* kernelPageTable;

static void vmm_load_memory_map(EfiMemoryMap* memoryMap)
{
    kernelPageTable = page_table_new();

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        page_table_map_pages(kernelPageTable, desc->virtualStart, desc->physicalStart, desc->amountOfPages, 
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

void* vmm_allocate(void* address, uint64_t size, uint64_t flags)
{
    if (address == NULL)
    {
        return NULLPTR(EFAULT);
    }

    Space* space = &sched_process()->space;

    address = (void*)ROUND_DOWN((uint64_t)address, PAGE_SIZE);

    for (uint64_t i = 0; i < SIZE_IN_PAGES(size); i++)
    {                    
        LOCK_GUARD(&space->lock);

        if (page_table_physical_address(space->pageTable, address) == NULL)
        {
            //Page table takes ownership of memory
            page_table_map(space->pageTable, address, pmm_allocate(), flags);
        }

        address = (void*)((uint64_t)address + PAGE_SIZE);
    }

    return address;
}

void* vmm_identity_map(void* address, uint64_t size, uint64_t flags)
{
    return vmm_map(VMM_LOWER_TO_HIGHER(address), address, size, flags);
}

void* vmm_map(void* virtualAddress, void* physicalAddress, uint64_t size, uint64_t flags)
{
    if (virtualAddress == NULL || physicalAddress == NULL)
    {
        return NULLPTR(EFAULT);
    }

    Space* space = &sched_process()->space;
    
    virtualAddress = (void*)ROUND_DOWN(virtualAddress, PAGE_SIZE);
    physicalAddress = (void*)ROUND_DOWN(physicalAddress, PAGE_SIZE);

    for (uint64_t i = 0; i < SIZE_IN_PAGES(size); i++)
    {                    
        LOCK_GUARD(&space->lock);

        if (page_table_physical_address(space->pageTable, virtualAddress) == NULL)
        {
            page_table_map(space->pageTable, virtualAddress, physicalAddress, flags);
        }

        virtualAddress = (void*)((uint64_t)virtualAddress + PAGE_SIZE);
        physicalAddress = (void*)((uint64_t)physicalAddress + PAGE_SIZE);
    }

    return virtualAddress;
}

void* vmm_unmap(void* virtualAddress, uint64_t size)
{
    debug_panic("vmm_unmap not implemented");
}

void* vmm_physical_to_virtual(const void* address)
{
    Space* space = &sched_process()->space;
    LOCK_GUARD(&space->lock);

    return page_table_physical_address(space->pageTable, address);
}