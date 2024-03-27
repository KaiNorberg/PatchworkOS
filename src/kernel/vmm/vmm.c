#include "vmm.h"

#include "utils/utils.h"
#include "heap/heap.h"
#include "lock/lock.h"
#include "pmm/pmm.h"
#include "sched/sched.h"
#include "registers/registers.h"

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

void vmm_init(EfiMemoryMap* memoryMap)
{
    vmm_load_memory_map(memoryMap);

    vmm_deallocate_boot_page_table(memoryMap);
}

void* vmm_map(void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    void* virtualAddress = VMM_LOWER_TO_HIGHER(physicalAddress);
    page_table_map_pages(kernelPageTable, virtualAddress, physicalAddress, pageAmount, 
        flags | VMM_KERNEL_PAGE_FLAGS);

    return virtualAddress;
}

void vmm_change_flags(void* address, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        page_table_change_flags(kernelPageTable, (void*)((uint64_t)address + i * PAGE_SIZE), 
            flags | VMM_KERNEL_PAGE_FLAGS);
    }
}

Space* space_new(void)
{
    Space* space = kmalloc(sizeof(Space));
    space->pageTable = page_table_new();
    space->lock = lock_create();

    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pageTable->entries[i] = kernelPageTable->entries[i];
    }
    return space;
}

void space_free(Space* space)
{
    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pageTable->entries[i] = 0;
    }

    page_table_free(space->pageTable);
    kfree(space);
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

void* space_allocate(Space* space, void* address, uint64_t pageAmount)
{
    if ((uint64_t)address + pageAmount * PAGE_SIZE > VMM_LOWER_HALF_MAX)
    {
        return NULLPTR(EFAULT);
    }
    else if (address == NULL)
    {
        //TODO: Choose address
        return NULLPTR(EFAULT);
    }

    address = (void*)round_down((uint64_t)address, PAGE_SIZE);

    for (uint64_t i = 0; i < pageAmount; i++)
    {            
        void* virtualAddress = (void*)((uint64_t)address + i * PAGE_SIZE);

        lock_acquire(&space->lock);
        if (page_table_physical_address(space->pageTable, virtualAddress) == NULL)
        {
            //Page table takes ownership of memory
            page_table_map(space->pageTable, virtualAddress, pmm_allocate(), 
                PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);
        }
        lock_release(&space->lock);
    }

    return address;
}

void* space_physical_to_virtual(Space* space, void* address)
{
    lock_acquire(&space->lock);
    void* temp = page_table_physical_address(space->pageTable, address);
    lock_release(&space->lock);
    return temp;
}