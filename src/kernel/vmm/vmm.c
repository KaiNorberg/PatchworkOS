#include "vmm.h"

#include <stdint.h>
#include <common/boot_info/boot_info.h>

#include "utils/utils.h"
#include "pmm/pmm.h"

extern uint64_t _kernelEnd;

static PageDirectory* kernelPageDirectory;
static uintptr_t topAddress;

static void vmm_load_memory_map(EfiMemoryMap* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize)); 
        
        page_directory_map_pages(kernelPageDirectory, desc->virtualStart, desc->physicalStart, desc->amountOfPages, PAGE_FLAG_WRITE | VMM_KERNEL_PAGE_FLAGS);
	}
    page_directory_populate_range(kernelPageDirectory, PAGE_DIRECTORY_ENTRY_AMOUNT / 2, PAGE_DIRECTORY_ENTRY_AMOUNT, PAGE_FLAG_WRITE | VMM_KERNEL_PAGE_FLAGS);
}

static void vmm_deallocate_boot_page_directory(EfiMemoryMap* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));

		if (desc->type == EFI_MEMORY_TYPE_PAGE_DIRECTORY)
		{
            pmm_free_pages(desc->physicalStart, desc->amountOfPages);
		}
	}
}

void vmm_init(EfiMemoryMap* memoryMap)
{
    //TODO: Enable cr4 page global flag

    topAddress = round_up((uint64_t)&_kernelEnd, PAGE_SIZE);
    kernelPageDirectory = page_directory_new();
    
    vmm_load_memory_map(memoryMap);

    vmm_deallocate_boot_page_directory(memoryMap);

    PAGE_DIRECTORY_LOAD(kernelPageDirectory);
}

void* vmm_physical_to_virtual(void* address)
{
    return (void*)((uint64_t)address + VMM_HIGHER_HALF_BASE);
}

void* vmm_virtual_to_physical(void* address)
{
    return (void*)((uint64_t)address - VMM_HIGHER_HALF_BASE);
}

PageDirectory* vmm_kernel_directory()
{
    return kernelPageDirectory;
}

void* vmm_allocate(uint64_t pageAmount, uint16_t flags)
{
    void* address = (void*)topAddress;

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        page_directory_map(kernelPageDirectory, (void*)topAddress, pmm_allocate(), flags | VMM_KERNEL_PAGE_FLAGS);
        topAddress += PAGE_SIZE;
    }

    return address;
}

void* vmm_map(void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    void* virtualAddress = vmm_physical_to_virtual(physicalAddress);
    page_directory_map_pages(kernelPageDirectory, virtualAddress, physicalAddress, pageAmount, flags | VMM_KERNEL_PAGE_FLAGS);

    return virtualAddress;
}

void vmm_map_kernel(PageDirectory* pageDirectory)
{
    page_directory_copy_range(pageDirectory, kernelPageDirectory, PAGE_DIRECTORY_ENTRY_AMOUNT / 2, PAGE_DIRECTORY_ENTRY_AMOUNT);
}