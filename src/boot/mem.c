#include "mem.h"
#include "efidef.h"

#include <common/defs.h>
#include <common/paging_types.h>
#include <efilib.h>

#include <boot/boot_info.h>
#include <common/paging.h>
#include <sys/proc.h>

// We cant use the normal memory allocator after exiting boot services so we use this basic one instead.
struct
{
    EFI_PHYSICAL_ADDRESS buffer;
    uint64_t pagesAllocated;
    boot_gop_t* gop;
    boot_memory_map_t* map;
} basicAllocator;

EFI_STATUS mem_init(void)
{
    Print(L"Initializing basic allocator... ");

    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData,
        MEM_BASIC_ALLOCATOR_MAX_PAGES, &basicAllocator.buffer);
    if (EFI_ERROR(status))
    {
        Print(L"failed to allocate buffer (0x%lx)!\n", status);
        return status;
    }

    basicAllocator.pagesAllocated = 0;
    basicAllocator.gop = NULL;
    basicAllocator.map = NULL;

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

NORETURN static void panic_without_boot_services(void)
{
    // Getting here would be bad, we have exited boot services so we cant print to the screen, and are out of
    // memory.
    memset32(basicAllocator.gop->physAddr, 0xFF0000, basicAllocator.gop->size / sizeof(uint32_t));
    for (;;)
    {
        asm("cli; hlt");
    }
}

static void* basic_allocator_alloc(void)
{
    if (basicAllocator.pagesAllocated == MEM_BASIC_ALLOCATOR_MAX_PAGES)
    {
        panic_without_boot_services();
    }

    return (void*)((uintptr_t)basicAllocator.buffer + (basicAllocator.pagesAllocated++) * EFI_PAGE_SIZE);
}

void mem_page_table_init(page_table_t* table, boot_memory_map_t* map, boot_gop_t* gop, boot_kernel_t* kernel)
{
    // TODO: There appears to be weird issues with paging in the bootloader as loading the page table appears to freeze
    // (probably a triple fault). Currently those issues are seemingly fixed, but i have been unable to determine the
    // actual cause of those previous issues so it might reccur, in the future the root cause will need to be
    // determined, for now just dont touch anything.

    if (page_table_init(table, basic_allocator_alloc, NULL) == ERR)
    {
        panic_without_boot_services();
    }

    pml_t* cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    for (uint64_t i = 0; i < PML_ENTRY_AMOUNT / 2; i++)
    {
        table->pml4->entries[i] = cr3->entries[i];
    }

    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);
        if (page_table_map(table, PML_LOWER_TO_HIGHER(desc->PhysicalStart), (void*)desc->PhysicalStart,
                desc->NumberOfPages, PML_WRITE, PML_CALLBACK_NONE) == ERR)
        {
            panic_without_boot_services();
        }
    }

    if (page_table_map(table, (void*)kernel->virtStart, (void*)kernel->physStart, BYTES_TO_PAGES(kernel->size),
            PML_WRITE, PML_CALLBACK_NONE) == ERR)
    {
        panic_without_boot_services();
    }

    if (page_table_map(table, (void*)gop->virtAddr, (void*)gop->physAddr, BYTES_TO_PAGES(gop->size), PML_WRITE,
            PML_CALLBACK_NONE) == ERR)
    {
        panic_without_boot_services();
    }
}
