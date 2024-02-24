#pragma once

#include <stdint.h>
#include <efi.h>
#include <efilib.h>
#include <common/boot_info/boot_info.h>

#include "efidef.h"
#include "x86_64/efibind.h"

#define PSF_MAGIC 1078

void pst_font_load(EFI_HANDLE imageHandle, PsfFont* font, CHAR16* path);