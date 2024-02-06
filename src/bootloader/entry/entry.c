#include <efi.h>
#include <efilib.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "rsdt/rsdt.h"
#include "loader/loader.h"
#include "ram_disk/ram_disk.h"
#include "file_system/file_system.h"

#include "../common.h"

EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
	InitializeLib(imageHandle, imageHandle);
	Print(L"Hello from the bootloader!\n\r");

	BootPage* bootPage = memory_allocate_pages(1, EFI_MEMORY_TYPE_BOOTPAGE);

	gop_get_framebuffer(&bootPage->screenbuffer);
	pst_font_load(imageHandle, &bootPage->font, L"/fonts/zap-vga16.psf");

	ram_disk_load_directory(&bootPage->ramRoot, file_system_open_root_volume(imageHandle), "root");

	bootPage->rsdp = rsdt_get(systemTable);
	bootPage->runtimeServices = systemTable->RuntimeServices;

	loader_load_kernel(imageHandle, systemTable, bootPage);

	return EFI_SUCCESS;
}
