#pragma once

#include <efi.h>
#include <efilib.h>

#include <stdint.h>
#include <sys/gfx.h>

#include <common/boot_info.h>

#define PSF_MAGIC 1078

void psf_font_load(psf_t* font, CHAR16* path, EFI_HANDLE imageHandle);
