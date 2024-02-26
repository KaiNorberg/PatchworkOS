#include <efi.h>
#include <efilib.h>
#include <common/boot_info/boot_info.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "rsdp/rsdp.h"
#include "loader/loader.h"
#include "ram_disk/ram_disk.h"
#include "file_system/file_system.h"
#include "memory/memory.h"
#include "virtual_memory/virtual_memory.h"

EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
	InitializeLib(imageHandle, imageHandle);
	Print(L"Hello from the bootloader!\n\r");

	virtual_memory_init();

	BootInfo* bootInfo = virtual_memory_allocate_pool(sizeof(BootInfo), EFI_MEMORY_TYPE_BOOT_INFO);

	gop_get_buffer(&bootInfo->gopBuffer);
	psf_font_load(&bootInfo->font, L"/fonts/zap-vga16.psf", imageHandle);
	bootInfo->ramRoot = ram_disk_load(imageHandle);
	bootInfo->rsdp = rsdp_get(systemTable);
	bootInfo->runtimeServices = systemTable->RuntimeServices;

	void* kernelEntry = load_kernel(L"/boot/kernel.elf", imageHandle);

	virtual_memory_map_populate(&bootInfo->memoryMap);

	Print(L"Jumping to kernel...\n");
	systemTable->BootServices->ExitBootServices(imageHandle, bootInfo->memoryMap.key);

	jump_to_kernel(kernelEntry, bootInfo);

	Print(L"If you are reading this then something has gone very wrong!");

	return EFI_ABORTED;
}
