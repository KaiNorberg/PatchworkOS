#include "disk.h"

#include "efilib.h"

#include <boot/boot_info.h>

#include <sys/list.h>

static void boot_file_free(boot_file_t* file)
{
    if (file != NULL)
    {
        if (file->data != NULL)
        {
            FreePool(file->data);
        }
        FreePool(file);
    }
}

static void boot_dir_free(boot_dir_t* dir)
{
    if (dir == NULL)
    {
        return;
    }

    while (!list_is_empty(&dir->children))
    {
        list_entry_t* entry = list_pop_first(&dir->children);
        boot_dir_t* child = CONTAINER_OF(entry, boot_dir_t, entry);
        boot_dir_free(child);
    }

    while (!list_is_empty(&dir->files))
    {
        list_entry_t* entry = list_pop_first(&dir->files);
        boot_file_t* file = CONTAINER_OF(entry, boot_file_t, entry);
        boot_file_free(file);
    }

    FreePool(dir);
}

static boot_file_t* ram_disk_load_file(EFI_FILE* volume, const CHAR16* path)
{
    if (volume == NULL || path == NULL)
    {
        return NULL;
    }

    EFI_FILE* efiFile;
    EFI_STATUS status = uefi_call_wrapper(volume->Open, 5, volume, &efiFile, path, EFI_FILE_MODE_READ,
        EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);
    if (EFI_ERROR(status))
    {
        return NULL;
    }

    boot_file_t* file = AllocatePool(sizeof(boot_file_t));
    if (file == NULL)
    {
        uefi_call_wrapper(efiFile->Close, 1, efiFile);
        return NULL;
    }

    list_entry_init(&file->entry);
    SetMem(file->name, MAX_NAME, 0);
    for (size_t i = 0; i < MAX_NAME - 1 && path[i] != '\0'; i++)
    {
        file->name[i] = (char)path[i];
    }
    file->data = NULL;
    file->size = 0;

    EFI_FILE_INFO* fileInfo = LibFileInfo(efiFile);
    if (fileInfo == NULL)
    {
        boot_file_free(file);
        uefi_call_wrapper(efiFile->Close, 1, efiFile);
        return NULL;
    }

    file->size = fileInfo->FileSize;
    FreePool(fileInfo);

    if (file->size > 0)
    {
        file->data = AllocatePool(file->size);
        if (file->data == NULL)
        {
            boot_file_free(file);
            uefi_call_wrapper(efiFile->Close, 1, efiFile);
            return NULL;
        }

        status = uefi_call_wrapper(efiFile->Read, 3, efiFile, &file->size, file->data);
        if (EFI_ERROR(status))
        {
            boot_file_free(file);
            uefi_call_wrapper(efiFile->Close, 1, efiFile);
            return NULL;
        }
    }

    uefi_call_wrapper(efiFile->Close, 1, efiFile);
    return file;
}

static boot_dir_t* disk_load_dir(EFI_FILE* volume, const CHAR16* name)
{
    if (volume == NULL || name == NULL)
    {
        return NULL;
    }

    boot_dir_t* dir = AllocatePool(sizeof(boot_dir_t));
    if (dir == NULL)
    {
        return NULL;
    }

    list_entry_init(&dir->entry);
    SetMem(dir->name, MAX_NAME, 0);
    for (size_t i = 0; i < MAX_NAME - 1 && name[i] != '\0'; i++)
    {
        dir->name[i] = (char)name[i];
    }
    list_init(&dir->children);
    list_init(&dir->files);

    while (1)
    {
        EFI_FILE_INFO* fileInfo;
        UINTN fileInfoSize = 0;

        EFI_STATUS status = uefi_call_wrapper(volume->Read, 3, volume, &fileInfoSize, NULL);
        if (status != EFI_BUFFER_TOO_SMALL)
        {
            break;
        }

        fileInfo = AllocatePool(fileInfoSize);
        if (fileInfo == NULL)
        {
            boot_dir_free(dir);
            return NULL;
        }

        status = uefi_call_wrapper(volume->Read, 3, volume, &fileInfoSize, fileInfo);
        if (EFI_ERROR(status))
        {
            FreePool(fileInfo);
            boot_dir_free(dir);
            return NULL;
        }

        if (fileInfo->Attribute & EFI_FILE_DIRECTORY)
        {
            if (StrCmp(fileInfo->FileName, L".") != 0 && StrCmp(fileInfo->FileName, L"..") != 0)
            {
                EFI_FILE* childVolume;
                status = uefi_call_wrapper(volume->Open, 5, volume, &childVolume, fileInfo->FileName,
                    EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);
                if (EFI_ERROR(status))
                {
                    FreePool(fileInfo);
                    boot_dir_free(dir);
                    return NULL;
                }

                boot_dir_t* child = disk_load_dir(childVolume, fileInfo->FileName);
                uefi_call_wrapper(childVolume->Close, 1, childVolume);

                if (child == NULL)
                {
                    FreePool(fileInfo);
                    boot_dir_free(dir);
                    return NULL;
                }

                list_push_back(&dir->children, &child->entry);
            }
        }
        else
        {
            boot_file_t* file = ram_disk_load_file(volume, fileInfo->FileName);
            if (file == NULL)
            {
                FreePool(fileInfo);
                boot_dir_free(dir);
                return NULL;
            }
            list_push_back(&dir->files, &file->entry);
        }

        FreePool(fileInfo);
    }

    return dir;
}

EFI_STATUS disk_load(boot_disk_t* disk, EFI_FILE* rootHandle)
{
    if (disk == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Loading disk... ");

    disk->root = disk_load_dir(rootHandle, L"root");
    if (disk->root == NULL)
    {
        Print(L"failed to load root directory!\n");
        return EFI_LOAD_ERROR;
    }

    Print(L"done!\n");
    return EFI_SUCCESS;
}
