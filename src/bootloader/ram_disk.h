#pragma once

#include <bootloader/boot_info.h>
#include <efi.h>
#include <efilib.h>

ram_dir_t* ram_disk_load(EFI_HANDLE imageHandle);

ram_file_t* ram_disk_load_file(EFI_FILE* volume, CHAR16* path);

ram_dir_t* ram_disk_load_directory(EFI_FILE* volume, const char* name);
