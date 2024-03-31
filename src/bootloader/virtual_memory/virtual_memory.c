#include "virtual_memory.h"

#include "page_table/page_table.h"
#include "memory/memory.h"

static PageTable* pageTable;

static void* kernelAddress;

void virtual_memory_init(void)
{
    kernelAddress = 0;
    pageTable = page_table_new();

    //Lower half must be mapped identically to maintain compatibility with all uefi implementations.
    PageTable* cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    for (uint64_t i = 0; i < 256; i++)
    {
        pageTable->entries[i] = cr3->entries[i];
    }

    EfiMemoryMap memoryMap;
    memory_map_populate(&memoryMap);

    for (uint64_t i = 0; i < memoryMap.descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap.base + (i * memoryMap.descriptorSize));
        
        void* virtualAddress = (void*)(HIGHER_HALF_BASE + desc->PhysicalStart);
        page_table_map_pages(pageTable, virtualAddress, (void*)desc->PhysicalStart, desc->NumberOfPages, PAGE_FLAG_WRITE);
	}

    PAGE_TABLE_LOAD(pageTable);
}

void virtual_memory_allocate_kernel(void* virtualAddress, uint64_t pageAmount)
{
    void* physicalAddress = memory_allocate_pages(pageAmount, EFI_MEMORY_TYPE_KERNEL);
    kernelAddress = virtualAddress;

    page_table_map_pages(pageTable, virtualAddress, physicalAddress, pageAmount, PAGE_FLAG_WRITE);
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
    memoryMap->base = (void*)(HIGHER_HALF_BASE + (uint64_t)memoryMap->base);

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        
        if (desc->Type == EFI_MEMORY_TYPE_KERNEL)
        {
            desc->VirtualStart = (uint64_t)kernelAddress;
        }
        else
        {
            desc->VirtualStart = HIGHER_HALF_BASE + desc->PhysicalStart;
        }
	}
}
