#include "fs.h"

#include <stddef.h>

#include "efilib.h"

EFI_STATUS fs_open_root_volume(EFI_FILE** file, EFI_HANDLE imageHandle)
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

EFI_STATUS fs_open(EFI_FILE** file, EFI_FILE* volume, CHAR16* name)
{
    return uefi_call_wrapper(volume->Open, 5, volume, file, name, EFI_FILE_MODE_READ,
        EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);
}

void fs_close(EFI_FILE* file)
{
    uefi_call_wrapper(file->Close, 1, file);
}

EFI_STATUS fs_seek(EFI_FILE* file, uint64_t offset)
{
    return uefi_call_wrapper(file->SetPosition, 2, file, offset);
}

EFI_STATUS fs_read(EFI_FILE* file, uint64_t readSize, void* buffer)
{
    return uefi_call_wrapper(file->Read, 3, file, &readSize, buffer);
}
