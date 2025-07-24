#pragma once

#include <efi.h>
#include <efilib.h>

#include <boot/boot_info.h>

EFI_STATUS kernel_load(boot_kernel_t* kernel, EFI_HANDLE imageHandle);
