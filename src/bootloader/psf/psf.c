#include "psf.h"

#include "file_system/file_system.h"

void pst_font_load(EFI_HANDLE imageHandle, PSFFont* font, CHAR16* path)
{
	Print(L"Loading Font... ");

	EFI_FILE* file = file_system_open(imageHandle, path);

	if (file == NULL)
	{
		Print(L"ERROR: Failed to load font!\n\r");
		
		while (1)
		{
			__asm__("HLT");
		}
	}

	PSFHeader* fontHeader = file_system_read(file, sizeof(PSFHeader));

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

	file_system_seek(file, sizeof(PSFHeader));
	void* glyphBuffer = file_system_read(file, glyphBufferSize);

	font->header = fontHeader;
	font->glyphs = glyphBuffer;

	file_system_close(file);
    /*char* glyph = font->glyphs + 'M' * 16;

	for (uint64_t y = 0; y < 64; y++)
	{
		for (uint64_t x = 0; x < 8; x++)
		{
			if ((*glyph & (0b10000000 >> x)) > 0)
			{
				Print(L"1");
			}
			else
			{
				Print(L"0");
			}
		}
		Print(L"\n\r");
		glyph++;
	}*/

	Print(L"Done!\n\r");

	Print(L"FONT INFO\n\r");
	Print(L"Char Size: %d\n\r", font->header->charSize);
	Print(L"Mode: %d\n\r", font->header->mode);
	Print(L"GlyphBuffer: 0x%x\n\r", glyphBuffer);
	Print(L"FONT INFO END\n\r");
}