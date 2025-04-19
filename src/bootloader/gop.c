#include "gop.h"

#include <stddef.h>
#include <stdint.h>

#include "efilib.h"
#include "vm.h"

static void gop_select_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL* gop, int64_t width, int64_t height)
{
    uint64_t bestMatch = UINT64_MAX;
    uint64_t bestDistance = UINT64_MAX;
    for (uint64_t i = 0; i < gop->Mode->MaxMode; ++i)
    {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
        uint64_t size;
        uefi_call_wrapper(gop->QueryMode, 4, gop, i, &size, &info);

        // Note: The distance is squared but it does not matter as we are only doing comparisons.
        int64_t xOffset = (int64_t)info->HorizontalResolution - width;
        int64_t yOffset = (int64_t)info->VerticalResolution - height;
        uint64_t distance = xOffset * xOffset + yOffset * yOffset;
        if (distance < bestDistance)
        {
            bestMatch = i;
            bestDistance = distance;
        }
    }

    uefi_call_wrapper(gop->SetMode, 2, gop, bestMatch);
}

void gop_buffer_init(gop_buffer_t* buffer)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status = uefi_call_wrapper(gBS->LocateProtocol, 3, &guid, 0, (void**)&gop);

    if (EFI_ERROR(status))
    {
        Print(L"ERROR: Failed to locate GOP!\n\r");

        while (1)
        {
            asm volatile("hlt");
        }
    }

    gop_select_mode(gop, GOP_WIDTH, GOP_HEIGHT);
    buffer->base = (uint32_t*)gop->Mode->FrameBufferBase;
    buffer->size = gop->Mode->FrameBufferSize;
    buffer->width = gop->Mode->Info->HorizontalResolution;
    buffer->height = gop->Mode->Info->VerticalResolution;
    buffer->stride = gop->Mode->Info->PixelsPerScanLine;

    Print(L"GOP BUFFER INFO\n\r");
    Print(L"Base: 0x%lx\n\r", buffer->base);
    Print(L"Size: 0x%lx\n\r", buffer->size);
    Print(L"Width: %d\n\r", buffer->width);
    Print(L"Height: %d\n\r", buffer->height);
    Print(L"PixelsPerScanline: %d\n\r", buffer->stride);
    Print(L"GOP BUFFER INFO END\n\r");
}
