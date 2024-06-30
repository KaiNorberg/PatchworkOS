#include "ram_disk.h"

#include <stddef.h>
#include <string.h>

#include "char16.h"
#include "fs.h"
#include "sys/list.h"
#include "vm.h"

static ram_file_t* ram_disk_load_file(EFI_FILE* volume, CHAR16* path)
{
    EFI_FILE* fileHandle = fs_open_raw(volume, path);

    ram_file_t* file = vm_alloc(sizeof(ram_file_t));
    list_entry_init(&file->base);
    char16_to_char(path, file->name);
    file->size = fs_get_size(fileHandle);
    file->data = vm_alloc(file->size);
    fs_read(fileHandle, file->size, file->data);

    fs_close(fileHandle);

    return file;
}

static ram_dir_t* ram_disk_load_directory(EFI_FILE* volume, const char* name)
{
    ram_dir_t* dir = vm_alloc(sizeof(ram_dir_t));
    list_entry_init(&dir->base);
    strcpy(dir->name, name);
    list_init(&dir->children);
    list_init(&dir->files);

    while (1)
    {
        EFI_FILE_INFO* fileInfo;
        UINTN fileInfoSize = 0;

        EFI_STATUS status = uefi_call_wrapper(volume->Read, 3, volume, &fileInfoSize, 0);
        if (status != EFI_BUFFER_TOO_SMALL)
        {
            break;
        }

        fileInfo = AllocatePool(fileInfoSize);

        status = fs_read(volume, fileInfoSize, fileInfo);
        if (EFI_ERROR(status))
        {
            Print(L"Error reading file info\n");
            FreePool(fileInfo);
            break;
        }

        if (fileInfo->Attribute & EFI_FILE_DIRECTORY)
        {
            if (StrCmp(fileInfo->FileName, L".") != 0 && StrCmp(fileInfo->FileName, L"..") != 0)
            {
                EFI_FILE_PROTOCOL* childVolume = fs_open_raw(volume, fileInfo->FileName);

                char childName[32];
                char16_to_char(fileInfo->FileName, childName);
                ram_dir_t* child = ram_disk_load_directory(childVolume, childName);

                list_push(&dir->children, child);

                fs_close(childVolume);
            }
        }
        else
        {
            ram_file_t* file = ram_disk_load_file(volume, fileInfo->FileName);

            list_push(&dir->files, file);
        }

        FreePool(fileInfo);
    }

    return dir;
}

ram_dir_t* ram_disk_load(EFI_HANDLE imageHandle)
{
    EFI_FILE* rootHandle = fs_open_root_volume(imageHandle);

    ram_dir_t* root = ram_disk_load_directory(rootHandle, "root");

    fs_close(rootHandle);

    return root;
}
