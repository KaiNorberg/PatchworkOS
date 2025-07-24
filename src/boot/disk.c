#include "disk.h"

#include "char16.h"
#include "efilib.h"
#include "fs.h"

#include <boot/boot_info.h>

#include <string.h>
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
        list_entry_t* entry = list_pop(&dir->children);
        boot_dir_t* child = CONTAINER_OF(entry, boot_dir_t, entry);
        boot_dir_free(child);
    }

    while (!list_is_empty(&dir->files))
    {
        list_entry_t* entry = list_pop(&dir->files);
        boot_file_t* file = CONTAINER_OF(entry, boot_file_t, entry);
        boot_file_free(file);
    }

    FreePool(dir);
}

static boot_file_t* ram_disk_load_file(EFI_FILE* volume, CHAR16* path)
{
    if (volume == NULL || path == NULL)
    {
        return NULL;
    }

    EFI_FILE* efiFile;
    EFI_STATUS status = fs_open(&efiFile, volume, (CHAR16*)path);
    if (EFI_ERROR(status))
    {
        return NULL;
    }

    char name[MAX_NAME];
    char16_to_char(path, name);

    boot_file_t* file = AllocatePool(sizeof(boot_file_t));
    if (file == NULL)
    {
        fs_close(efiFile);
        return NULL;
    }

    list_entry_init(&file->entry);
    strncpy(file->name, name, MAX_NAME - 1);
    file->name[MAX_NAME - 1] = '\0';
    file->data = NULL;
    file->size = 0;

    EFI_FILE_INFO* fileInfo = LibFileInfo(efiFile);
    if (fileInfo == NULL)
    {
        boot_file_free(file);
        fs_close(efiFile);
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
            fs_close(efiFile);
            return NULL;
        }

        status = fs_read(efiFile, file->size, file->data);
        if (EFI_ERROR(status))
        {
            boot_file_free(file);
            fs_close(efiFile);
            return NULL;
        }
    }

    fs_close(efiFile);
    return file;
}

static boot_dir_t* disk_load_dir(EFI_FILE* volume, const char* name)
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
    strncpy(dir->name, name, MAX_NAME - 1);
    dir->name[MAX_NAME - 1] = '\0';
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
                status = fs_open(&childVolume, volume, fileInfo->FileName);
                if (EFI_ERROR(status))
                {
                    FreePool(fileInfo);
                    boot_dir_free(dir);
                    return NULL;
                }

                char childName[MAX_NAME];
                char16_to_char(fileInfo->FileName, childName);

                boot_dir_t* child = disk_load_dir(childVolume, childName);
                fs_close(childVolume);

                if (child == NULL)
                {
                    FreePool(fileInfo);
                    boot_dir_free(dir);
                    return NULL;
                }

                list_push(&dir->children, &child->entry);
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
            list_push(&dir->files, &file->entry);
        }

        FreePool(fileInfo);
    }

    return dir;
}

EFI_STATUS disk_load(boot_disk_t* disk, EFI_HANDLE imageHandle)
{
    if (disk == NULL || imageHandle == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    Print(L"Loading disk... ");

    EFI_FILE* rootHandle;
    EFI_STATUS status = fs_open_root_volume(&rootHandle, imageHandle);
    if (EFI_ERROR(status))
    {
        Print(L"failed to open root volume (0x%x)!\n", status);
        return EFI_LOAD_ERROR;
    }

    disk->root = disk_load_dir(rootHandle, "root");
    fs_close(rootHandle);

    if (disk->root == NULL)
    {
        Print(L"failed to load root directory (0x%x)!\n", status);
        return EFI_LOAD_ERROR;
    }

    Print(L"done!\n");
    return EFI_SUCCESS;
}
