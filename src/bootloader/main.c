#include <efi.h>
#include <efilib.h>
#include <common/boot_info.h>

#include "gop.h"
#include "psf.h"
#include "rsdp.h"
#include "loader.h"
#include "ram_disk.h"
#include "fs.h"
#include "memory.h"
#include "vm.h"

void* boot_info_populate(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable, BootInfo* bootInfo)
{
    gop_buffer_init(&bootInfo->gopBuffer);
    psf_font_load(&bootInfo->font, L"/usr/fonts/zap-vga16.psf", imageHandle);
    bootInfo->ramRoot = ram_disk_load(imageHandle);
    bootInfo->rsdp = rsdp_get(systemTable);
    bootInfo->runtimeServices = systemTable->RuntimeServices;

    void* entry = load_kernel(L"/boot/kernel.elf", imageHandle);

    vm_map_init(&bootInfo->memoryMap);

    return entry;
}

EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
    InitializeLib(imageHandle, imageHandle);
    Print(L"Hello from the bootloader!\n\r");

    vm_init();

    BootInfo* bootInfo = vm_alloc(sizeof(BootInfo), EFI_MEMORY_TYPE_BOOT_INFO);
    void* entry = boot_info_populate(imageHandle, systemTable, bootInfo);

    jump_to_kernel(entry, bootInfo);

    Print(L"If you are reading this then something has gone very wrong!");
    return EFI_ABORTED;
}
