#include "mem_map.h"

#include <bootloader/boot_info.h>

#include <string.h>

void mem_map_init(efi_mem_map_t* memoryMap)
{
    memset(memoryMap, 0, sizeof(efi_mem_map_t));

    memoryMap->base = LibMemoryMap(&memoryMap->descriptorAmount, &memoryMap->key, &memoryMap->descriptorSize,
        &memoryMap->descriptorVersion);
    if (memoryMap->base == NULL)
    {
        Print(L"ERROR: Unable to get memory map!");

        while (1)
        {
            asm volatile("hlt");
        }
    }
}

void mem_map_deinit(efi_mem_map_t* memoryMap)
{
    FreePool(memoryMap->base);
}
