#include "ram_disk.h"

#include <stddef.h>
#include <string.h>

#include "char16.h"
#include "fs.h"
#include "vm.h"

ram_dir_t* ram_disk_load(EFI_HANDLE imageHandle)
{
    EFI_FILE* rootHandle = fs_open_root_volume(imageHandle);

    ram_dir_t* root = ram_disk_load_directory(rootHandle, "root");

    fs_close(rootHandle);

    return root;
}

ram_file_t* ram_disk_load_file(EFI_FILE* volume, CHAR16* path)
{
    EFI_FILE* fileHandle = fs_open_raw(volume, path);

    ram_file_t* file = vm_alloc(sizeof(ram_file_t), EFI_MEM_RAM_DISK);

    file->size = fs_get_size(fileHandle);
    file->data = vm_alloc(file->size, EFI_MEM_RAM_DISK);
    fs_read(fileHandle, file->size, file->data);

    SetMem(file->name, 32, 0);
    char16_to_char(path, file->name);

    fs_close(fileHandle);

    return file;
}

ram_dir_t* ram_disk_load_directory(EFI_FILE* volume, const char* name)
{
    ram_dir_t* dir = vm_alloc(sizeof(ram_dir_t), EFI_MEM_RAM_DISK);

    SetMem(dir->name, 32, 0);
    strcpy(dir->name, name);
    dir->firstFile = 0;
    dir->lastFile = 0;
    dir->firstChild = 0;
    dir->lastChild = 0;
    dir->next = 0;
    dir->prev = 0;

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

                if (dir->firstChild == 0)
                {
                    child->next = 0;
                    child->prev = 0;

                    dir->firstChild = child;
                    dir->lastChild = child;
                }
                else
                {
                    child->next = 0;
                    child->prev = dir->lastChild;

                    dir->lastChild->next = child;
                    dir->lastChild = child;
                }

                fs_close(childVolume);
            }
        }
        else
        {
            ram_file_t* file = ram_disk_load_file(volume, fileInfo->FileName);

            if (dir->firstFile == 0)
            {
                file->next = 0;
                file->prev = 0;

                dir->firstFile = file;
                dir->lastFile = file;
            }
            else
            {
                file->next = 0;
                file->prev = dir->lastFile;

                dir->lastFile->next = file;
                dir->lastFile = file;
            }
        }

        FreePool(fileInfo);
    }

    return dir;
}
