#pragma once

#include <efi.h>
#include <efilib.h>

#include <stdint.h>

EFI_FILE* fs_open_root_volume(EFI_HANDLE imageHandle);

EFI_FILE* fs_open_raw(EFI_FILE* volume, CHAR16* path);

EFI_FILE* fs_open(CHAR16* path, EFI_HANDLE imageHandle);

void fs_close(EFI_FILE* file);

EFI_STATUS fs_read(EFI_FILE* file, uint64_t readSize, void* buffer);

void fs_seek(EFI_FILE* file, uint64_t offset);

uint64_t fs_get_size(EFI_FILE* file);
