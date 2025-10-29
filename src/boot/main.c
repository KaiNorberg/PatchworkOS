#include "disk.h"
#include "gop.h"
#include "kernel.h"
#include "mem.h"

#include <boot/boot_info.h>
#include <kernel/mem/paging.h>
#include <kernel/version.h>

#include <efi.h>
#include <efilib.h>

#define EXIT_BOOT_SERVICES_MAX_RETRY 5

static void splash_screen(void)
{
#ifdef NDEBUG
    Print(L"Start %a-bootloader %a (Built %a %a)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#else
    Print(L"Start %a-bootloader DEBUG %a (Built %a %a)\n", OS_NAME, OS_VERSION, __DATE__, __TIME__);
#endif
    Print(L"Copyright (C) 2025 Kai Norberg. MIT Licensed. See /usr/license/LICENSE for details.\n");
}

static EFI_STATUS open_root_volume(EFI_FILE** file, EFI_HANDLE imageHandle)
{
    EFI_LOADED_IMAGE* loaded_image = 0;
    EFI_GUID lipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_FILE_IO_INTERFACE* IOVolume;
    EFI_GUID fsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    EFI_STATUS status = uefi_call_wrapper(BS->HandleProtocol, 3, imageHandle, &lipGuid, (void**)&loaded_image);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle, &fsGuid, (VOID*)&IOVolume);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = uefi_call_wrapper(IOVolume->OpenVolume, 2, IOVolume, file);
    if (EFI_ERROR(status))
    {
        return status;
    }

    return EFI_SUCCESS;
}

static void* rsdp_get(EFI_SYSTEM_TABLE* systemTable)
{
    Print(L"Searching for RSDP... ");
    EFI_CONFIGURATION_TABLE* configTable = systemTable->ConfigurationTable;
    EFI_GUID acpi2TableGuid = ACPI_20_TABLE_GUID;

    void* rsdp = NULL;
    for (uint64_t i = 0; i < systemTable->NumberOfTableEntries; i++)
    {
        if (CompareGuid(&configTable[i].VendorGuid, &acpi2TableGuid) &&
            CompareMem("RSD PTR ", configTable->VendorTable, 8) == 0)
        {
            rsdp = configTable->VendorTable;
        }
        configTable++;
    }

    if (rsdp == NULL)
    {
        Print(L"failed to locate rsdp!\n");
    }
    else
    {
        Print(L"found at %p!\n", rsdp);
    }
    return rsdp;
}

static EFI_STATUS boot_info_populate(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable, boot_info_t* bootInfo)
{
    EFI_STATUS status = gop_buffer_init(&bootInfo->gop);
    if (EFI_ERROR(status))
    {
        return status;
    }

    bootInfo->rsdp = rsdp_get(systemTable);
    if (bootInfo->rsdp == NULL)
    {
        return EFI_NOT_FOUND;
    }

    bootInfo->runtimeServices = systemTable->RuntimeServices;

    EFI_FILE* rootHandle;
    status = open_root_volume(&rootHandle, imageHandle);
    if (EFI_ERROR(status))
    {
        Print(L"failed to open root volume (0x%x)!\n", status);
        return EFI_LOAD_ERROR;
    }

    status = disk_load(&bootInfo->disk, rootHandle);
    if (EFI_ERROR(status))
    {
        return status;
    }

    status = kernel_load(&bootInfo->kernel, rootHandle);
    if (EFI_ERROR(status))
    {
        return status;
    }

    // Memory map retrived when exiting boot services.

    return EFI_SUCCESS;
}

static EFI_STATUS exit_boot_services(EFI_HANDLE imageHandle, boot_info_t* bootInfo)
{
    EFI_STATUS status;

    uint32_t retries = 0;
    do
    {
        Print(L"Exiting boot services (attempt %d)...\n", retries + 1);

        if (retries > 0)
        {
            mem_map_deinit(&bootInfo->memory.map);
        }

        status = mem_map_init(&bootInfo->memory.map);
        if (EFI_ERROR(status))
        {
            Print(L"Failed to initialize memory map (0x%x)!\n", status);
            return status;
        }

        status = uefi_call_wrapper(BS->ExitBootServices, 2, imageHandle, bootInfo->memory.map.key);
        if (EFI_ERROR(status))
        {
            if (status == EFI_INVALID_PARAMETER)
            {
                retries++;
                if (retries >= EXIT_BOOT_SERVICES_MAX_RETRY)
                {
                    Print(L"Too many retries!\n");
                    mem_map_deinit(&bootInfo->memory.map);
                    return EFI_ABORTED;
                }

                uefi_call_wrapper(BS->Stall, 1, 1000);
            }
            else
            {
                mem_map_deinit(&bootInfo->memory.map);
                Print(L"Failed to exit boot services (0x%x)!\n", status);
                return status;
            }
        }
    } while (status == EFI_INVALID_PARAMETER && retries < EXIT_BOOT_SERVICES_MAX_RETRY);

    mem_page_table_init(&bootInfo->memory.table, &bootInfo->memory.map, &bootInfo->gop, &bootInfo->kernel);

    return EFI_SUCCESS;
}

EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
    InitializeLib(imageHandle, systemTable);

    splash_screen();

    EFI_STATUS status = mem_init();
    if (EFI_ERROR(status))
    {
        Print(L"Failed to initialize memory (0x%x)!\n", status);
        return status;
    }

    boot_info_t* bootInfo = AllocatePool(sizeof(boot_info_t));
    if (bootInfo == NULL)
    {
        Print(L"Failed to allocate boot info!\n");
        return EFI_OUT_OF_RESOURCES;
    }

    status = boot_info_populate(imageHandle, systemTable, bootInfo);
    if (EFI_ERROR(status))
    {
        Print(L"Failed to populate boot info (0x%x)!\n", status);
        return status;
    }

    status = exit_boot_services(imageHandle, bootInfo);
    if (EFI_ERROR(status))
    {
        Print(L"Failed to exit boot services (0x%x)!\n", status);
        return status;
    }

    page_table_load(&bootInfo->memory.table);
    bootInfo->kernel.entry(bootInfo);

    // We should never end up back here.
    return EFI_ABORTED;
}
