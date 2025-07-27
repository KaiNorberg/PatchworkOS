#include "gop.h"
#include "mem.h"

#include <boot/boot_info.h>
#include <common/paging_types.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/proc.h>

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

EFI_STATUS gop_buffer_init(boot_gop_t* buffer)
{
    Print(L"Locating GOP... ");
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop;
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &guid, 0, (void**)&gop);
    if (EFI_ERROR(status))
    {
        Print(L"failed to locate GOP!\n");
        return status;
    }

#if !(GOP_USE_DEFAULT_RES)
    gop_select_mode(gop, GOP_WIDTH, GOP_HEIGHT);
#endif
    buffer->physAddr = (uint32_t*)gop->Mode->FrameBufferBase;
    buffer->virtAddr = PML_LOWER_TO_HIGHER(buffer->physAddr);
    buffer->size = gop->Mode->FrameBufferSize;
    buffer->width = gop->Mode->Info->HorizontalResolution;
    buffer->height = gop->Mode->Info->VerticalResolution;
    buffer->stride = gop->Mode->Info->PixelsPerScanLine;
    Print(L"located buffer width=%d, height=%d, stride=%d... ", buffer->width, buffer->height, buffer->stride);

    Print(L"done!\n");
    return EFI_SUCCESS;
}
