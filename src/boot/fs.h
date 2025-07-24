#pragma once

#include <efi.h>
#include <efilib.h>

#include <stdint.h>

EFI_STATUS fs_open_root_volume(EFI_FILE** file, EFI_HANDLE imageHandle);

EFI_STATUS fs_open(EFI_FILE** file, EFI_FILE* volume, CHAR16* name);

void fs_close(EFI_FILE* file);

EFI_STATUS fs_seek(EFI_FILE* file, uint64_t offset);

EFI_STATUS fs_read(EFI_FILE* file, uint64_t readSize, void* buffer);
