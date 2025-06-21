#include "fs.h"

#include <stddef.h>

#include "efilib.h"

EFI_FILE* fs_open_root_volume(EFI_HANDLE imageHandle)
{
    EFI_LOADED_IMAGE* loaded_image = 0;
    EFI_GUID lipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_FILE_IO_INTERFACE* IOVolume;
    EFI_GUID fsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_FILE* volume;

    uefi_call_wrapper(BS->HandleProtocol, 3, imageHandle, &lipGuid, (void**)&loaded_image);
    uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle, &fsGuid, (VOID*)&IOVolume);
    uefi_call_wrapper(IOVolume->OpenVolume, 2, IOVolume, &volume);

    return volume;
}

EFI_FILE* fs_open_raw(EFI_FILE* volume, CHAR16* path)
{
    EFI_FILE* fileHandle;

    uefi_call_wrapper(volume->Open, 5, volume, &fileHandle, path, EFI_FILE_MODE_READ,
        EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);

    return fileHandle;
}

EFI_FILE* fs_open(CHAR16* path, EFI_HANDLE imageHandle)
{
    if (StrLen(path) < 3)
    {
        return 0;
    }

    EFI_FILE* currentVolume = fs_open_root_volume(imageHandle);

    uint64_t index = 1;
    uint64_t prevIndex = 1;
    if (path[0] == '/')
    {
        while (1)
        {
            index++;

            if (path[index] == L'/')
            {
                CHAR16* nameStart = path + prevIndex;
                CHAR16* nameEnd = path + index;
                uint64_t nameLength = ((uint64_t)nameEnd - (uint64_t)nameStart) / 2;

                CHAR16* name = AllocatePool((nameLength + 1) * sizeof(CHAR16));
                CopyMem(name, nameStart, nameLength * sizeof(CHAR16));
                name[nameLength] = 0;

                EFI_FILE* oldVolume = currentVolume;

                currentVolume = fs_open_raw(oldVolume, name);

                if (prevIndex != 1)
                {
                    fs_close(oldVolume);
                }

                FreePool(name);

                prevIndex = index + 1;
            }
            else if (path[index] == L'\0')
            {
                CHAR16* nameStart = path + prevIndex;
                CHAR16* nameEnd = path + index;
                uint64_t nameLength = ((uint64_t)nameEnd - (uint64_t)nameStart) / 2;

                CHAR16* name = AllocatePool((nameLength + 1) * sizeof(CHAR16));
                CopyMem(name, nameStart, nameLength * sizeof(CHAR16));
                name[nameLength] = 0;

                EFI_FILE* file = fs_open_raw(currentVolume, name);

                if (prevIndex != 1)
                {
                    fs_close(currentVolume);
                }

                FreePool(name);

                return file;
            }
        }
    }

    return 0;
}

void fs_seek(EFI_FILE* file, uint64_t offset)
{
    uefi_call_wrapper(file->SetPosition, 2, file, offset);
}

EFI_STATUS fs_read(EFI_FILE* file, uint64_t readSize, void* buffer)
{
    return uefi_call_wrapper(file->Read, 3, file, &readSize, buffer);
}

void fs_close(EFI_FILE* file)
{
    uefi_call_wrapper(file->Close, 1, file);
}

uint64_t fs_get_size(EFI_FILE* file)
{
    EFI_FILE_INFO* fileInfo = LibFileInfo(file);
    uint64_t ret = fileInfo->FileSize;
    FreePool(fileInfo);
    return ret;
}
