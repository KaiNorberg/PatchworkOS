#pragma once

#include <stdint.h>

#include <efi.h>
#include <efilib.h>

#define PSF_MAGIC 1078

typedef struct
{
	uint16_t magic;
	uint8_t mode;
	uint8_t charSize;
} PSFHeader;

typedef struct
{
	PSFHeader* header;
	void* glyphs;
} PSFFont;

void pst_font_load(EFI_HANDLE imageHandle, PSFFont* font, CHAR16* path);