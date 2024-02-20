#include "vmm.h"

#include "utils/utils.h"
#include "pmm/pmm.h"
#include "memory/memory.h"
#include "tty/tty.h"

#include <stdint.h>
#include <libc/string.h>
#include <common/boot_info/boot_info.h>

extern uint64_t _kernelEnd;

static PageDirectory* kernelPageDirectory;
static uintptr_t nextFreeAddress;

void vmm_init(EfiMemoryMap* memoryMap)
{
    nextFreeAddress = round_up((uint64_t)&_kernelEnd, 0x1000);

    kernelPageDirectory = page_directory_new();

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        
        if (desc->type != EFI_MEMORY_TYPE_KERNEL)
        {
            page_directory_map_pages(kernelPageDirectory, (void*)((uint64_t)desc->physicalStart + VMM_PHYSICAL_BASE), desc->physicalStart, desc->amountOfPages, PAGE_FLAG_READ_WRITE);
        }
        else        
        {            
            page_directory_map_pages(kernelPageDirectory, desc->virtualStart, desc->physicalStart, desc->amountOfPages, PAGE_FLAG_READ_WRITE);
        }
	}

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));

		if (desc->type == EFI_MEMORY_TYPE_PAGE_DIRECTORY)
		{
            pmm_unlock_pages(desc->physicalStart, desc->amountOfPages);
		}
	}

    PAGE_DIRECTORY_LOAD(kernelPageDirectory);
}

void* vmm_physical_to_virtual(void* address)
{
    return (void*)((uint64_t)address + (uint64_t)pmm_physical_base());
}

PageDirectory* vmm_kernel_directory()
{
    return kernelPageDirectory;
}

void* vmm_request_memory(uint64_t pageAmount, uint16_t flags)
{
    void* address = (void*)nextFreeAddress;
    nextFreeAddress += pageAmount * 0x1000;

    page_directory_map_pages(kernelPageDirectory, address, pmm_request_amount(pageAmount), pageAmount, flags);

    return address;
}

void* vmm_request_address(void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    void* virtualAddress = vmm_physical_to_virtual(physicalAddress);
    page_directory_map_pages(kernelPageDirectory, virtualAddress, physicalAddress, pageAmount, flags);

    return virtualAddress;
}

void vmm_map_kernel(PageDirectory* pageDirectory)
{
    return;
}