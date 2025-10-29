#pragma once

#include <boot/boot_info.h>
#include <efi.h>
#include <efilib.h>

EFI_STATUS disk_load(boot_disk_t* disk, EFI_FILE* rootHandle);
