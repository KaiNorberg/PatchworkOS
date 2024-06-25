#include "psf.h"

#include <stddef.h>

#include "fs.h"
#include "vm.h"

void psf_font_load(psf_t* font, CHAR16* path, EFI_HANDLE imageHandle)
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

    psf_header_t header;
    fs_read(file, sizeof(psf_header_t), &header);

    if (header.magic != PSF_MAGIC)
    {
        Print(L"ERROR: Invalid font magic found (%d)!\n\r", header.magic);

        while (1)
        {
            asm volatile("hlt");
        }
    }

    uint64_t glyphsSize = header.charSize * 256;

    void* glyphBuffer = vm_alloc(glyphsSize, EFI_BOOT_INFO);
    fs_seek(file, sizeof(psf_header_t));
    fs_read(file, glyphsSize, glyphBuffer);

    font->glyphs = glyphBuffer;

    fs_close(file);

    Print(L"FONT INFO\n\r");
    Print(L"Char Size: %d\n\r", header.charSize);
    Print(L"Mode: %d\n\r", header.mode);
    Print(L"GlyphBuffer: 0x%lx\n\r", glyphBuffer);
    Print(L"FONT INFO END\n\r");
}
