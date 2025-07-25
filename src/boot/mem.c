#include "mem.h"
#include "efidef.h"
#include "efierr.h"

#include <common/defs.h>
#include <common/paging_types.h>
#include <efilib.h>

#include <boot/boot_info.h>
#include <common/paging.h>
#include <sys/proc.h>

static void* page_table_alloc_page(void)
{
    EFI_PHYSICAL_ADDRESS address;
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiBootServicesData, 1, &address);
    if (EFI_ERROR(status))
    {
        return NULL;
    }
    
    return (void*)address;
}

static void page_table_free_page(void* page)
{
    if (page == NULL)
    {
        return;
    }

    uefi_call_wrapper(BS->FreePages, 1, (EFI_PHYSICAL_ADDRESS)page, 1);
}

EFI_STATUS mem_page_table_init(page_table_t* table)
{
    Print(L"Initializing page table... ");

    if (page_table_init(table, page_table_alloc_page, page_table_free_page) == ERR)
    {
        Print(L"failed to initialize page table!\n");
        return EFI_OUT_OF_RESOURCES;
    }

    boot_memory_map_t map;
    EFI_STATUS status = mem_map_init(&map);
    if (status != EFI_SUCCESS)
    {
        Print(L"failed to initialize memory map (0x%x)!\n", status);
        return status;
    }

    pml_t* cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    for (uint64_t i = 0; i < 256; i++)
    {
        table->pml4->entries[i] = cr3->entries[i];
    }

    for (uint64_t i = 0; i < map.length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(&map, i);
        if (page_table_map(table, PML_LOWER_TO_HIGHER(desc->PhysicalStart), (void*)desc->PhysicalStart,
                desc->NumberOfPages, PML_WRITE, PML_CALLBACK_NONE) == ERR)
        {
            Print(L"failed to map memory region (0x%x)!\n", status);
            mem_map_deinit(&map);
            return EFI_OUT_OF_RESOURCES;
        }
    }

    mem_map_deinit(&map);
    Print(L"done!\n");
    return EFI_SUCCESS;
}

EFI_STATUS mem_map_init(boot_memory_map_t* map)
{
    EFI_STATUS status = EFI_SUCCESS;

    map->descriptors = LibMemoryMap(&map->length, &map->key, &map->descSize, &map->descVersion);
    if (map->descriptors == NULL)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    for (size_t i = 0; i < map->length; i++)
    {
        EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);
        desc->VirtualStart = (EFI_VIRTUAL_ADDRESS)PML_LOWER_TO_HIGHER(desc->PhysicalStart);
    }

    return status;
}

void mem_map_deinit(boot_memory_map_t* map)
{
    FreePool(map->descriptors);
    map->descriptors = NULL;
    map->length = 0;
}

EFI_STATUS mem_page_table_map_gop_kernel(page_table_t* table, boot_memory_map_t* map, boot_gop_t* gop, boot_kernel_t* kernel)
{
    Print(L"Mapping GOP and kernel memory... ");

    if (page_table_map(table, (void*)kernel->virtStart, (void*)kernel->physStart, BYTES_TO_PAGES(kernel->size),
            PML_WRITE, PML_CALLBACK_NONE) == ERR)
    {
        Print(L"failed to map kernel memory!\n");
        return EFI_OUT_OF_RESOURCES;
    }

    if (page_table_map(table, (void*)gop->virtAddr, (void*)gop->physAddr, BYTES_TO_PAGES(gop->size), PML_WRITE,
            PML_CALLBACK_NONE) == ERR)
    {
        Print(L"failed to map gop memory!\n");
        return EFI_OUT_OF_RESOURCES;
    }

    return EFI_SUCCESS;
}
