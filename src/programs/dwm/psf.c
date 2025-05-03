#include "psf.h"

#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>

static font_id_t newId = 0;

static psf_t* psf1_load(fd_t file, uint32_t desiredHeight)
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

    psf_t* psf = malloc(sizeof(psf_t) + glyphBufferSize);
    if (psf == NULL)
    {
        return NULL;
    }
    list_entry_init(&psf->entry);
    psf->id = newId++;
    psf->width = 8;
    psf->height = header.glyphSize;
    psf->scale = MAX(1, desiredHeight / psf->height);
    psf->glyphSize = header.glyphSize;
    psf->glyphAmount = glyphAmount;
    if (read(file, psf->glyphs, glyphBufferSize) != glyphBufferSize)
    {
        free(psf);
        return NULL;
    }

    return psf;
}

static psf_t* psf2_load(fd_t file, uint32_t desiredHeight)
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

    psf_t* psf = malloc(sizeof(psf_t) + glyphBufferSize);
    if (psf == NULL)
    {
        return NULL;
    }
    list_entry_init(&psf->entry);
    psf->id = newId++;
    psf->width = header.width;
    psf->height = header.height;
    psf->scale = MAX(1, desiredHeight / psf->height);
    psf->glyphSize = header.glyphSize;
    psf->glyphAmount = header.glyphAmount;
    if (read(file, psf->glyphs, glyphBufferSize) != glyphBufferSize)
    {
        free(psf);
        return NULL;
    }

    return psf;
}

psf_t* psf_new(const char* path, uint32_t desiredHeight)
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

    psf_t* psf;
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
        psf = NULL;
    }

    close(file);
    return psf;
}

void psf_free(psf_t* psf)
{
    free(psf);
}
