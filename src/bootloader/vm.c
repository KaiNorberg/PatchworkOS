#include "vm.h"

#include "memory.h"
#include "page_table.h"

static PageTable* pageTable;

static void* kernelAddress;

void vm_init(void)
{
    kernelAddress = 0;
    pageTable = page_table_new();

    // Lower half must be mapped identically to maintain compatibility with all uefi implementations.
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

        void* virtAddr = (void*)(HIGHER_HALF_BASE + desc->PhysicalStart);
        page_table_map_pages(pageTable, virtAddr, (void*)desc->PhysicalStart, desc->NumberOfPages, PAGE_FLAG_WRITE);
    }

    memory_map_cleanup(&memoryMap);
    PAGE_TABLE_LOAD(pageTable);
}

void vm_alloc_kernel(void* virtAddr, uint64_t pageAmount)
{
    void* physAddr = memory_allocate_pages(pageAmount, EFI_MEMORY_TYPE_KERNEL);
    kernelAddress = virtAddr;

    page_table_map_pages(pageTable, virtAddr, physAddr, pageAmount, PAGE_FLAG_WRITE);
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
