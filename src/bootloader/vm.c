#include "vm.h"

#include "page_table.h"
#include "memory.h"

static PageTable* pageTable;

static void* kernelAddress;

void vm_init(void)
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
    memory_map_init(&memoryMap);

    for (uint64_t i = 0; i < memoryMap.descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap.base + (i * memoryMap.descriptorSize));
        
        void* virtualAddress = (void*)(HIGHER_HALF_BASE + desc->PhysicalStart);
        page_table_map_pages(pageTable, virtualAddress, (void*)desc->PhysicalStart, desc->NumberOfPages, PAGE_FLAG_WRITE);
	}

    memory_map_cleanup(&memoryMap);
    PAGE_TABLE_LOAD(pageTable);
}

void vm_alloc_kernel(void* virtualAddress, uint64_t pageAmount)
{
    void* physicalAddress = memory_allocate_pages(pageAmount, EFI_MEMORY_TYPE_KERNEL);
    kernelAddress = virtualAddress;

    page_table_map_pages(pageTable, virtualAddress, physicalAddress, pageAmount, PAGE_FLAG_WRITE);
}

void* vm_alloc(uint64_t size, uint64_t memoryType)
{
    return (void*)((uint64_t)memory_allocate_pool(size, memoryType) + HIGHER_HALF_BASE);
}

void vm_map_init(EfiMemoryMap* memoryMap)
{
    memory_map_init(memoryMap);

    void* newBuffer = vm_alloc(memoryMap->descriptorAmount * memoryMap->descriptorSize, EFI_MEMORY_TYPE_MEMORY_MAP);
    CopyMem(newBuffer, memoryMap->base, memoryMap->descriptorAmount * memoryMap->descriptorSize);

    memory_map_cleanup(memoryMap);
    memoryMap->base = newBuffer;

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
        
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
