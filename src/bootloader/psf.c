#include "psf.h"

#include <stddef.h>

#include "fs.h"
#include "vm.h"

void psf_font_load(PsfFont* font, CHAR16* path, EFI_HANDLE imageHandle)
{
    EFI_FILE* file = fs_open(path, imageHandle);

    if (file == 0)
	{
	    Print(L"ERROR: Failed to load font!\n\r");
		
	    while (1)
		{
		    asm volatile("hlt");
		}
	}

    fs_read(file, sizeof(PsfHeader), &font->header);

    if (font->header.magic != PSF_MAGIC)
	{
	    Print(L"ERROR: Invalid font magic found (%d)!\n\r", font->header.magic);
		
	    while (1)
		{
		    asm volatile("hlt");
		}
	}

    if (font->header.mode == 1)
	{
	    font->glyphsSize = font->header.charSize * 512;
	}
    else
	{
	    font->glyphsSize = font->header.charSize * 256;
	}

    void* glyphBuffer = vm_alloc(font->glyphsSize, EFI_MEMORY_TYPE_BOOT_INFO);
    fs_seek(file, sizeof(PsfHeader));
    fs_read(file, font->glyphsSize, glyphBuffer);

    font->glyphs = glyphBuffer;

    fs_close(file);

    Print(L"FONT INFO\n\r");
    Print(L"Char Size: %d\n\r", font->header.charSize);
    Print(L"Mode: %d\n\r", font->header.mode);
    Print(L"GlyphBuffer: 0x%lx\n\r", glyphBuffer);
    Print(L"FONT INFO END\n\r");
}