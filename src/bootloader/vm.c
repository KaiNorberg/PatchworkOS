#include "vm.h"

#include "mem.h"
#include "pml.h"

static pml_t* pageTable;

static void* kernelAddress;

void vm_init(void)
{
    kernelAddress = 0;
    pageTable = pml_new();

    // Lower half must be mapped identically to maintain compatibility with all uefi implementations.
    pml_t* cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    for (uint64_t i = 0; i < 256; i++)
    {
        pageTable->entries[i] = cr3->entries[i];
    }

    efi_mem_map_t memoryMap;
    mem_map_init(&memoryMap);

    for (uint64_t i = 0; i < memoryMap.descriptorAmount; i++)
    {
        efi_mem_desc_t* desc = (efi_mem_desc_t*)((uint64_t)memoryMap.base + (i * memoryMap.descriptorSize));

        void* virtAddr = (void*)(HIGHER_HALF_BASE + desc->PhysicalStart);
        pml_map_pages(pageTable, virtAddr, (void*)desc->PhysicalStart, desc->NumberOfPages, PAGE_WRITE);
    }

    mem_map_cleanup(&memoryMap);
    PML_LOAD(pageTable);
}

void vm_alloc_kernel(void* virtAddr, uint64_t pageAmount)
{
    void* physAddr = mem_alloc_pages(pageAmount, EFI_KERNEL_MEMORY);
    kernelAddress = virtAddr;

    pml_map_pages(pageTable, virtAddr, physAddr, pageAmount, PAGE_WRITE);
}

void* vm_alloc(uint64_t size, uint64_t memoryType)
{
    return (void*)((uint64_t)mem_alloc_pool(size, memoryType) + HIGHER_HALF_BASE);
}

void vm_map_init(efi_mem_map_t* memoryMap)
{
    mem_map_init(memoryMap);

    void* newBuffer = vm_alloc(memoryMap->descriptorAmount * memoryMap->descriptorSize, EFI_MEMORY_MAP);
    CopyMem(newBuffer, memoryMap->base, memoryMap->descriptorAmount * memoryMap->descriptorSize);

    mem_map_cleanup(memoryMap);
    memoryMap->base = newBuffer;

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (desc->Type == EFI_KERNEL_MEMORY)
        {
            desc->VirtualStart = (uint64_t)kernelAddress;
        }
        else
        {
            desc->VirtualStart = HIGHER_HALF_BASE + desc->PhysicalStart;
        }
    }
}
