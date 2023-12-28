#include <efi.h>
#include <efilib.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "rsdt/rsdt.h"
#include "loader/loader.h"
#include "ram_disk/ram_disk.h"
#include "file_system/file_system.h"

EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
	InitializeLib(imageHandle, imageHandle);
	Print(L"BootLoader loaded!\n\r");

	Framebuffer screenbuffer;
	gop_get_framebuffer(&screenbuffer);

	PSFFont font;
	pst_font_load(imageHandle, &font, L"/fonts/zap-vga16.psf");

	RamDiskDirectory ramDiskRoot = ram_disk_load_directory(file_system_open_root_volume(imageHandle), "root");

	BootInfo bootInfo;
	bootInfo.screenbuffer = &screenbuffer;
	bootInfo.font = &font;
	bootInfo.rsdp = rsdt_get(systemTable);
	bootInfo.runtimeServices = systemTable->RuntimeServices;
	bootInfo.ramDiskRoot = &ramDiskRoot;

	loader_load_kernel(imageHandle, systemTable, &bootInfo);

	return EFI_SUCCESS;
}
