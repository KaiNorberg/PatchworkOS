#pragma once

#include <efi.h>
#include <efilib.h>

#include <bootloader/boot_info.h>

void* loader_load_kernel(CHAR16* path, EFI_HANDLE imageHandle);
