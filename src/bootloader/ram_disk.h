#pragma once

#include <bootloader/boot_info.h>
#include <efi.h>
#include <efilib.h>

ram_dir_t* ram_disk_load(EFI_HANDLE imageHandle);
