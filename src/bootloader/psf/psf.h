#pragma once

#include <stdint.h>
#include <efi.h>
#include <efilib.h>
#include <common/boot_info/boot_info.h>

#include "efidef.h"
#include "x86_64/efibind.h"

#define PSF_MAGIC 1078

void psf_font_load(PsfFont* font, CHAR16* path, EFI_HANDLE imageHandle);