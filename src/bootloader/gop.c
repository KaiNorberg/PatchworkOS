#include "gop.h"

#include <stddef.h>
#include <stdint.h>

#include "vm.h"

void gop_buffer_init(GopBuffer* buffer)
{
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &guid, 0, (void**)&gop);

    if (EFI_ERROR(status))
    {
        Print(L"ERROR: Failed to locate GOP!\n\r");

        while (1)
        {
            asm volatile("hlt");
        }
    }

    buffer->base = (uint32_t*)gop->Mode->FrameBufferBase;
    buffer->size = gop->Mode->FrameBufferSize;
    buffer->width = gop->Mode->Info->HorizontalResolution;
    buffer->height = gop->Mode->Info->VerticalResolution;
    buffer->pixelsPerScanline = gop->Mode->Info->PixelsPerScanLine;

    Print(L"GOP BUFFER INFO\n\r");
    Print(L"Base: 0x%lx\n\r", buffer->base);
    Print(L"Size: 0x%lx\n\r", buffer->size);
    Print(L"Width: %d\n\r", buffer->width);
    Print(L"Height: %d\n\r", buffer->height);
    Print(L"PixelsPerScanline: %d\n\r", buffer->pixelsPerScanline);
    Print(L"GOP BUFFER INFO END\n\r");
}