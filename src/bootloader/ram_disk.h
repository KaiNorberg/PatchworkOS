#pragma once

#include <efi.h>
#include <efilib.h>
#include <common/boot_info.h>

RamDir* ram_disk_load(EFI_HANDLE imageHandle);

RamFile* ram_disk_load_file(EFI_FILE* volume, CHAR16* path);

RamDir* ram_disk_load_directory(EFI_FILE* volume, const char* name);
