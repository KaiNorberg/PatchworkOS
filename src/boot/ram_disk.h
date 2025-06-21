#pragma once

#include <boot/boot_info.h>
#include <efi.h>
#include <efilib.h>

void ram_disk_load(ram_disk_t* disk, EFI_HANDLE imageHandle);
