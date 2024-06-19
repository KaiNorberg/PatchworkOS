#include "mem.h"

void* mem_alloc_pages(uint64_t pageAmount, uint64_t memoryType)
{
    EFI_PHYSICAL_ADDRESS address = 0;
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAnyPages, memoryType, pageAmount, &address);
    if (EFI_ERROR(status))
    {
        Print(L"ERROR: Unable to allocate pages!");

        while (1)
        {
            asm volatile("hlt");
        }
    }

    return (void*)address;
}

void* mem_alloc_pool(uint64_t size, uint64_t memoryType)
{
    EFI_PHYSICAL_ADDRESS address = 0;
    EFI_STATUS status = uefi_call_wrapper(BS->AllocatePool, 4, memoryType, size, &address);
    if (EFI_ERROR(status))
    {
        Print(L"ERROR: Unable to allocate pages!");

        while (1)
        {
            asm volatile("hlt");
        }
    }

    return (void*)address;
}

void mem_free_pool(void* pool)
{
    uefi_call_wrapper(BS->FreePool, 1, pool);
}

void mem_map_init(efi_mem_map_t* memoryMap)
{
    memoryMap->base =
        LibMemoryMap(&memoryMap->descriptorAmount, &memoryMap->key, &memoryMap->descriptorSize, &memoryMap->descriptorVersion);
}

void mem_map_cleanup(efi_mem_map_t* memoryMap)
{
    mem_free_pool(memoryMap->base);
}
