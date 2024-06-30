#include "gop.h"
#include "loader.h"
#include "ram_disk.h"
#include "rsdp.h"
#include "vm.h"

#include <bootloader/boot_info.h>

#include <efi.h>
#include <efilib.h>

static void boot_info_populate(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable, boot_info_t* bootInfo)
{
    gop_buffer_init(&bootInfo->gopBuffer);
    bootInfo->ramRoot = ram_disk_load(imageHandle);
    bootInfo->rsdp = rsdp_get(systemTable);
    bootInfo->runtimeServices = systemTable->RuntimeServices;
    loader_load_kernel(&bootInfo->kernel, L"/boot/kernel", imageHandle);
    vm_map_init(&bootInfo->memoryMap);
}

EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
    InitializeLib(imageHandle, systemTable);
    Print(L"Hello from the bootloader!\n\r");

    vm_init();

    boot_info_t* bootInfo = vm_alloc(sizeof(boot_info_t));
    boot_info_populate(imageHandle, systemTable, bootInfo);

    EFI_STATUS status = uefi_call_wrapper(BS->ExitBootServices, 2, imageHandle, bootInfo->memoryMap.key);
    if (EFI_ERROR(status))
    {
        Print(L"ERROR: Failed to exit boot services (%r)\n\r", status);
        return status;
    }

    bootInfo->kernel.entry(bootInfo);

    return EFI_ABORTED;
}
