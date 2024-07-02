#include "vm.h"

#include "mem_map.h"
#include "pml.h"

#include <bootloader/boot_info.h>

static pml_t* pageTable;

void vm_init(void)
{
    pageTable = pml_new();

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

void* vm_alloc_pages(void* virtAddr, uint64_t pageAmount, uint32_t type)
{
    EFI_PHYSICAL_ADDRESS physAddr = 0;
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, type, pageAmount, &physAddr);
    if (EFI_ERROR(status))
    {
        Print(L"ERROR: Unable to allocate pages!");

        while (1)
        {
            asm volatile("hlt");
        }
    }

    pml_map_pages(pageTable, virtAddr, (void*)physAddr, pageAmount, PAGE_WRITE);

    return (void*)physAddr;
}

void* vm_alloc(uint64_t size)
{
    return (void*)((uint64_t)AllocatePool(size) + HIGHER_HALF_BASE);
}

void vm_map_init(efi_mem_map_t* memoryMap)
{
    mem_map_init(memoryMap);

    memoryMap->base = (void*)(HIGHER_HALF_BASE + (uint64_t)memoryMap->base);

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        desc->VirtualStart = HIGHER_HALF_BASE + desc->PhysicalStart;
    }
}
