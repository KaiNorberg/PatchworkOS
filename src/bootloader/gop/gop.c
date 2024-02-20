#include "gop.h"

void gop_get_buffer(GopBuffer* buffer)
{	
	EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
	EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &guid, NULL, (void**)&gop);

	if (EFI_ERROR(status))
	{
		Print(L"ERROR: gop Failed!\n\r");
		
		while (1)
		{
			__asm__("hlt");
		}
	}

	buffer->base = (uint32_t*)gop->Mode->FrameBufferBase;
	buffer->size = gop->Mode->FrameBufferSize;
	buffer->width = gop->Mode->Info->HorizontalResolution;
	buffer->height = gop->Mode->Info->VerticalResolution;
	buffer->pixelsPerScanline = gop->Mode->Info->PixelsPerScanLine;

	Print(L"GOP BUFFER INFO\n\r");
	Print(L"Base: 0x%x\n\r", buffer->base);
	Print(L"Size: 0x%x\n\r", buffer->size);
	Print(L"Width: %d\n\r", buffer->width);
	Print(L"Height: %d\n\r", buffer->height);
	Print(L"PixelsPerScanline: %d\n\r", buffer->pixelsPerScanline);
	Print(L"GOP BUFFER INFO END\n\r");
}