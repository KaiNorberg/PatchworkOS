#pragma once

#include <efi.h>
#include <efilib.h>
#include <stdint.h>

#include "efidef.h"
#include "efiprot.h"
#include "x86_64/efibind.h"

EFI_FILE* file_system_open_root_volume(EFI_HANDLE imageHandle);

EFI_FILE* file_system_open_raw(EFI_FILE* volume, CHAR16* path);

EFI_FILE* file_system_open(EFI_HANDLE imageHandle, CHAR16* path);

void file_system_seek(EFI_FILE* file, uint64_t offset);

EFI_STATUS file_system_read(EFI_FILE* file, uint64_t readSize, void* buffer);

void file_system_close(EFI_FILE* file);

uint64_t file_system_get_size(EFI_FILE* file);
