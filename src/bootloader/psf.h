#pragma once

#include <efi.h>
#include <efilib.h>

#include <stdint.h>

#include <common/boot_info.h>

#define PSF_MAGIC 1078

void psf_font_load(BootFont* font, CHAR16* path, EFI_HANDLE imageHandle);
