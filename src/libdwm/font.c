#include "internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <sys/io.h>

font_t* font_default(display_t* disp)
{
    return disp->defaultFont;
}

static font_t* psf1_load(fd_t file, uint32_t desiredHeight)
{
    struct
    {
        uint16_t magic;
        uint8_t mode;
        uint8_t glyphSize;
    } header;
    if (read(file, &header, sizeof(header)) != sizeof(header))
    {
        return NULL;
    }

    if (header.magic != PSF1_MAGIC)
    {
        return NULL;
    }

    uint64_t glyphAmount = header.mode & PSF1_MODE_512 ? 512 : 256;
    uint64_t glyphBufferSize = glyphAmount * header.glyphSize;

    font_t* psf = malloc(sizeof(font_t) + glyphBufferSize);
    if (psf == NULL)
    {
        return NULL;
    }
    list_entry_init(&psf->entry);
    psf->scale = MAX(1, desiredHeight / header.glyphSize);
    psf->width = 8 * psf->scale;
    psf->height = header.glyphSize * psf->scale;
    psf->glyphSize = header.glyphSize;
    psf->glyphAmount = glyphAmount;
    if (read(file, psf->glyphs, glyphBufferSize) != glyphBufferSize)
    {
        free(psf);
        return NULL;
    }

    return psf;
}

static font_t* psf2_load(fd_t file, uint32_t desiredHeight)
{
    struct
    {
        uint32_t magic;
        uint32_t version;
        uint32_t headerSize;
        uint32_t flags;
        uint32_t glyphAmount;
        uint32_t glyphSize;
        uint32_t height;
        uint32_t width;
    } header;
    if (read(file, &header, sizeof(header)) != sizeof(header))
    {
        return NULL;
    }

    if (header.magic != PSF2_MAGIC || header.version != 0 || header.headerSize != sizeof(header))
    {
        return NULL;
    }

    uint64_t glyphBufferSize = header.glyphAmount * header.glyphSize;

    font_t* psf = malloc(sizeof(font_t) + glyphBufferSize);
    if (psf == NULL)
    {
        return NULL;
    }
    list_entry_init(&psf->entry);
    psf->scale = MAX(1, desiredHeight / header.height);
    psf->width = header.width * psf->scale;
    psf->height = header.height * psf->scale;
    psf->glyphSize = header.glyphSize;
    psf->glyphAmount = header.glyphAmount;
    if (read(file, psf->glyphs, glyphBufferSize) != glyphBufferSize)
    {
        free(psf);
        return NULL;
    }

    return psf;
}

font_t* font_new(display_t* disp, const char* path, uint64_t desiredHeight)
{
    fd_t file = open(path);
    if (file == ERR)
    {
        return NULL;
    }

    uint8_t firstByte;
    if (read(file, &firstByte, sizeof(firstByte)) != sizeof(firstByte))
    {
        return NULL;
    }
    seek(file, 0, SEEK_SET);

    font_t* psf;
    if (firstByte == 0x36) // Is psf1
    {
        psf = psf1_load(file, desiredHeight);
    }
    else if (firstByte == 0x72) // Is psf2
    {
        psf = psf2_load(file, desiredHeight);
    }
    else
    {
        close(file);
        return NULL;
    }

    close(file);

    psf->disp = disp;
    list_push(&disp->fonts, &psf->entry);
    return psf;
}

void font_free(font_t* font)
{
    list_remove(&font->entry);
    free(font);
}

uint64_t font_height(font_t* font)
{
    return font->height;
}

uint64_t font_width(font_t* font)
{
    return font->width;
}
