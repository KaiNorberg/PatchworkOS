#include "virtual_memory.h"

#include "page_directory/page_directory.h"
#include "memory/memory.h"

static PageDirectory* pageDirectory;

static AllocatedAddress addresses[ALLOCATED_ADDRESS_MAX_AMOUNT];
static uint64_t addressAmount;

void virtual_memory_init()
{    
    SetMem(addresses, sizeof(AllocatedAddress) * ALLOCATED_ADDRESS_MAX_AMOUNT, 0);
    addressAmount = 0;

    pageDirectory = page_directory_new();

    //Lower half must be mapped identically to maintain compatibility with all uefi implementations.
    PageDirectory* cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    for (uint64_t i = 0; i < 256; i++)
    {
        pageDirectory->entries[i] = cr3->entries[i];
    }

    EfiMemoryMap memoryMap;
    memory_map_populate(&memoryMap);

    for (uint64_t i = 0; i < memoryMap.descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap.base + (i * memoryMap.descriptorSize));
        
        void* virtualAddress = (void*)(HIGHER_HALF_BASE + desc->PhysicalStart);
        page_directory_map_pages(pageDirectory, virtualAddress, (void*)desc->PhysicalStart, desc->NumberOfPages, PAGE_FLAG_WRITE);
	}

    PAGE_DIRECTORY_LOAD(pageDirectory);
}

void virtual_memory_allocate_address(void* virtualAddress, uint64_t pageAmount, uint64_t memoryType)
{
    void* physicalAddress = memory_allocate_pages(pageAmount, memoryType);
    
    addresses[addressAmount] = (AllocatedAddress)
    {
        .physicalAddress = physicalAddress,
        .virtualAddress = virtualAddress   
    };
    addressAmount++;

    page_directory_map_pages(pageDirectory, virtualAddress, physicalAddress, pageAmount, PAGE_FLAG_WRITE);
}

void* virtual_memory_allocate_pages(uint64_t pageAmount, uint64_t memoryType)
{
    return (void*)((uint64_t)memory_allocate_pages(pageAmount, memoryType) + HIGHER_HALF_BASE);
}

void* virtual_memory_allocate_pool(uint64_t size, uint64_t memoryType)
{
    return (void*)((uint64_t)memory_allocate_pool(size, memoryType) + HIGHER_HALF_BASE);
}

void virtual_memory_map_populate(EfiMemoryMap* memoryMap)
{
    memory_map_populate(memoryMap);
    memoryMap->base = (void*)((uint64_t)memoryMap->base + HIGHER_HALF_BASE);

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        
        uint8_t found = 0;
        for (uint64_t j = 0; j < addressAmount; j++)
        {
            if (desc->PhysicalStart == (uint64_t)addresses[j].physicalAddress)
            {
                desc->VirtualStart = (uint64_t)addresses[j].virtualAddress;
                found = 1;
                break;
            }
        }
        if (!found)
        {
            desc->VirtualStart = HIGHER_HALF_BASE + desc->PhysicalStart;
        }
	}
}
