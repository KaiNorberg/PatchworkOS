#include "vmm.h"

#include "utils/utils.h"
#include "heap/heap.h"
#include "lock/lock.h"
#include "pmm/pmm.h"
#include "scheduler/scheduler.h"
#include "registers/registers.h"

static PageDirectory* kernelPageDirectory;

static void vmm_load_memory_map(EfiMemoryMap* memoryMap)
{
    kernelPageDirectory = page_directory_new();

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        page_directory_map_pages(kernelPageDirectory, desc->virtualStart, desc->physicalStart, desc->amountOfPages, PAGE_FLAG_WRITE | VMM_KERNEL_PAGE_FLAGS);
	}

    page_directory_load(kernelPageDirectory);
}

static void vmm_deallocate_boot_page_directory(EfiMemoryMap* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

		if (desc->type == EFI_MEMORY_TYPE_PAGE_DIRECTORY)
		{
            pmm_free_pages(desc->physicalStart, desc->amountOfPages);
		}
	}
}

void vmm_init(EfiMemoryMap* memoryMap)
{
    vmm_load_memory_map(memoryMap);

    vmm_deallocate_boot_page_directory(memoryMap);
}

void* vmm_physical_to_virtual(void* address)
{
    return (void*)((uint64_t)address + VMM_HIGHER_HALF_BASE);
}

void* vmm_virtual_to_physical(void* address)
{
    return (void*)((uint64_t)address - VMM_HIGHER_HALF_BASE);
}

void* vmm_map(void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    void* virtualAddress = vmm_physical_to_virtual(physicalAddress);
    page_directory_map_pages(kernelPageDirectory, virtualAddress, physicalAddress, pageAmount, flags | VMM_KERNEL_PAGE_FLAGS);

    return virtualAddress;
}

void vmm_change_flags(void* address, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t i = 0; i < pageAmount; i++)
    {
        page_directory_change_flags(kernelPageDirectory, (void*)((uint64_t)address + i * PAGE_SIZE), flags | VMM_KERNEL_PAGE_FLAGS);
    }
}

AddressSpace* address_space_new(void)
{
    AddressSpace* space = kmalloc(sizeof(AddressSpace));
    space->pageDirectory = page_directory_new();
    space->lock = lock_create();

    for (uint64_t i = PDE_AMOUNT / 2; i < PDE_AMOUNT; i++)
    {
        space->pageDirectory->entries[i] = kernelPageDirectory->entries[i];
    }
    return space;
}

void address_space_free(AddressSpace* space)
{
    for (uint64_t i = PDE_AMOUNT / 2; i < PDE_AMOUNT; i++)
    {
        space->pageDirectory->entries[i] = 0;
    }

    page_directory_free(space->pageDirectory);
    kfree(space);
}

void address_space_load(AddressSpace* space)
{
    if (space == NULL)
    {
        page_directory_load(kernelPageDirectory);
    }
    else
    {
        page_directory_load(space->pageDirectory);
    }
}

void* address_space_allocate(AddressSpace* space, void* address, uint64_t pageAmount)
{
    if ((uint64_t)address >= VMM_LOWER_HALF_MAX)
    {
        scheduler_thread()->errno = EFAULT;
        return NULL;
    }

    //Todo: Map one page at a time, add check if page already mapped.

    void* physicalAddress = pmm_allocate_amount(pageAmount);

    //Page Directory takes ownership of memory.
    lock_acquire(&space->lock);
    page_directory_map_pages(space->pageDirectory, address, physicalAddress, pageAmount, PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);
    lock_release(&space->lock);

    return vmm_physical_to_virtual(physicalAddress);
}