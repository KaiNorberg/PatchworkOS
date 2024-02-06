#include "psf.h"

#include "file_system/file_system.h"
#include "memory/memory.h"

#include "../common.h"

void pst_font_load(EFI_HANDLE imageHandle, PSFFont* font, CHAR16* path)
{
	EFI_FILE* file = file_system_open(imageHandle, path);

	if (file == NULL)
	{
		Print(L"ERROR: Failed to load font!\n\r");
		
		while (1)
		{
			__asm__("HLT");
		}
	}

	PSFHeader* fontHeader = memory_allocate_pool(sizeof(PSFHeader), EFI_MEMORY_TYPE_SCREEN_FONT);
	file_system_read(file, sizeof(PSFHeader), fontHeader);

	if (fontHeader->magic != PSF_MAGIC)
	{
		Print(L"ERROR: Invalid font magic found (%d)!\n\r", fontHeader->magic);
		
		while (1)
		{
			__asm__("HLT");
		}
	}

	uint64_t glyphBufferSize = fontHeader->charSize * 256;
	if (fontHeader->mode == 1)
	{
		glyphBufferSize = fontHeader->charSize * 512;
	}

	void* glyphBuffer = memory_allocate_pool(glyphBufferSize, EFI_MEMORY_TYPE_SCREEN_FONT);
	file_system_seek(file, sizeof(PSFHeader));
	file_system_read(file, glyphBufferSize, glyphBuffer);

	font->header = fontHeader;
	font->glyphs = glyphBuffer;

	file_system_close(file);

	Print(L"FONT INFO\n\r");
	Print(L"Char Size: %d\n\r", font->header->charSize);
	Print(L"Mode: %d\n\r", font->header->mode);
	Print(L"GlyphBuffer: 0x%x\n\r", glyphBuffer);
	Print(L"FONT INFO END\n\r");
}