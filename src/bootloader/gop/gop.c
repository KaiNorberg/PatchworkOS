#include "gop.h"

void gop_get_framebuffer(Framebuffer* framebuffer)
{	
	EFI_GUID GOP_GUID = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* GOP;
	EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &GOP_GUID, NULL, (void**)&GOP);

	if (EFI_ERROR(status))
	{
		Print(L"ERROR: GOP Failed!\n\r");
		
		while (1)
		{
			__asm__("hlt");
		}
	}

	framebuffer->base = (uint32_t*)GOP->Mode->FrameBufferBase;
	framebuffer->size = GOP->Mode->FrameBufferSize;
	framebuffer->width = GOP->Mode->Info->HorizontalResolution;
	framebuffer->height = GOP->Mode->Info->VerticalResolution;
	framebuffer->pixelsPerScanline = GOP->Mode->Info->PixelsPerScanLine;

	Print(L"GOP BUFFER INFO\n\r");
	Print(L"Base: 0x%x\n\r", framebuffer->base);
	Print(L"Size: 0x%x\n\r", framebuffer->size);
	Print(L"Width: %d\n\r", framebuffer->width);
	Print(L"Height: %d\n\r", framebuffer->height);
	Print(L"PixelsPerScanline: %d\n\r", framebuffer->pixelsPerScanline);
	Print(L"GOP BUFFER INFO END\n\r");
}