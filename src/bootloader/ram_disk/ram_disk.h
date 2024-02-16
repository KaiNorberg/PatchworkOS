#pragma once

#include <efi.h>
#include <efilib.h>

#include <common/boot_info/boot_info.h>

RamFile* ram_disk_load_file(EFI_FILE* volume, CHAR16* path);

RamDirectory* ram_disk_load_directory(EFI_FILE* volume, const char* name);
