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
    uint64_t maxPages;
    uint64_t pagesAllocated;
    boot_gop_t* gop;
    boot_memory_map_t* map;
} basicAllocator;

EFI_STATUS mem_init(void)
{
    Print(L"Initializing basic allocator... ");

    boot_memory_map_t map;
    EFI_STATUS status = mem_map_init(&map);
    if (EFI_ERROR(status))
    {
        Print(L"failed to initialize memory map (0x%lx)!\n", status);
        return status;
    }

    uint64_t availPages = 0;
    for (size_t i = 0; i < map.length; i++)
    {
        EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(&map, i);
        if (desc->Type == EfiConventionalMemory)
        {
            availPages += desc->NumberOfPages;
        }
    }

    basicAllocator.maxPages =
        MAX(availPages * MEM_BASIC_ALLOCATOR_RESERVE_PERCENTAGE / 100, MEM_BASIC_ALLOCATOR_MIN_PAGES);
    Print(L"basic alloc using %llu pages... ", basicAllocator.maxPages);

    status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, basicAllocator.maxPages,
        &basicAllocator.buffer);
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

NORETURN static void panic_without_boot_services(uint8_t red, uint8_t green, uint8_t blue)
{
    // Getting here would be bad, we have exited boot services so we cant print to the screen, and are out of
    // memory. A better solution might be to implement a very basic logging system, but for now we just fill the screen
    // with a color and halt the CPU.
    memset32(basicAllocator.gop->physAddr, 0xFF000000 | (red << 16) | (green << 8) | (blue),
        basicAllocator.gop->size / sizeof(uint32_t));
    for (;;)
    {
        asm("cli; hlt");
    }
}

static void* basic_allocator_alloc(void)
{
    if (basicAllocator.pagesAllocated == basicAllocator.maxPages)
    {
        panic_without_boot_services(0xFF, 0x00, 0x00);
    }

    return (void*)((uintptr_t)basicAllocator.buffer + (basicAllocator.pagesAllocated++) * EFI_PAGE_SIZE);
}

void mem_page_table_init(page_table_t* table, boot_memory_map_t* map, boot_gop_t* gop, boot_kernel_t* kernel)
{
    basicAllocator.gop = gop;
    basicAllocator.map = map;

    if (page_table_init(table, basic_allocator_alloc, NULL) == ERR)
    {
        panic_without_boot_services(0x00, 0xFF, 0x00);
    }

    uintptr_t maxAddress = 0;
    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);
        maxAddress = MAX(maxAddress, desc->PhysicalStart + desc->NumberOfPages * PAGE_SIZE);
    }

    // I have no idea why its not enough to just identity map everything in the memory map, but this seems to be
    // required for paging to work properly on all platforms.
    if (page_table_map(table, 0, 0, BYTES_TO_PAGES(maxAddress), PML_WRITE | PML_PRESENT, PML_CALLBACK_NONE) == ERR)
    {
        panic_without_boot_services(0xFF, 0xFF, 0x00);
    }

    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);
        if (desc->VirtualStart < PML_HIGHER_HALF_START)
        {
            panic_without_boot_services(0x00, 0x00, 0xFF);
        }
        if (desc->PhysicalStart > PML_LOWER_HALF_END)
        {
            panic_without_boot_services(0xFF, 0x00, 0xFF);
        }

        if (page_table_map(table, (void*)desc->VirtualStart, (void*)desc->PhysicalStart, desc->NumberOfPages,
                PML_WRITE | PML_PRESENT, PML_CALLBACK_NONE) == ERR)
        {
            panic_without_boot_services(0xFF, 0x00, 0xFF);
        }
    }

    if (page_table_map(table, (void*)kernel->virtStart, (void*)kernel->physStart, BYTES_TO_PAGES(kernel->size),
            PML_WRITE | PML_PRESENT, PML_CALLBACK_NONE) == ERR)
    {
        panic_without_boot_services(0x00, 0xFF, 0xFF);
    }

    if (page_table_map(table, (void*)gop->virtAddr, (void*)gop->physAddr, BYTES_TO_PAGES(gop->size),
            PML_WRITE | PML_PRESENT, PML_CALLBACK_NONE) == ERR)
    {
        panic_without_boot_services(0xFF, 0xFF, 0xFF);
    }
}
