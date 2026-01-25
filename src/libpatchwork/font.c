#include "internal.h"

#include <stdlib.h>
#include <string.h>
#include <sys/fs.h>

font_t* font_default(display_t* disp)
{
    mtx_lock(&disp->mutex);
    font_t* temp = disp->defaultFont;
    mtx_unlock(&disp->mutex);
    return temp;
}

#include <stdio.h>

font_t* font_new(display_t* disp, const char* family, const char* weight, uint64_t size)
{
    const theme_t* theme = theme_global_get();
    if (strcmp(family, "default") == 0)
    {
        family = theme->defaultFont;
    }

    fd_t file = open(F("%s/%s-%s%d.grf", theme->fontsDir, family, weight, size));
    if (file == _FAIL)
    {
        return NULL;
    }

    uint64_t fileSize = seek(file, 0, SEEK_END);
    seek(file, 0, SEEK_SET);

    if (fileSize <= sizeof(font_t))
    {
        close(file);
        return NULL;
    }

    font_t* font = malloc(sizeof(font_t) - sizeof(grf_t) + fileSize);
    if (font == NULL)
    {
        close(file);
        return NULL;
    }

    grf_t grf;
    if (read(file, &font->grf, fileSize) != fileSize)
    {
        free(font);
        close(file);
        return NULL;
    }

    if (font->grf.magic != GRF_MAGIC)
    {
        free(font);
        close(file);
        return NULL;
    }

    for (uint64_t i = 0; i < 256; i++)
    {
        if (font->grf.glyphOffsets[i] != GRF_NONE && font->grf.glyphOffsets[i] >= fileSize)
        {
            free(font);
            close(file);
            return NULL;
        }
    }

    for (uint64_t i = 0; i < 256; i++)
    {
        if (font->grf.kernOffsets[i] != GRF_NONE && font->grf.kernOffsets[i] >= fileSize)
        {
            free(font);
            close(file);
            return NULL;
        }
    }

    close(file);
    font->disp = disp;
    list_entry_init(&font->entry);
    mtx_lock(&disp->mutex);
    list_push_back(&disp->fonts, &font->entry);
    mtx_unlock(&disp->mutex);
    return font;
}

void font_free(font_t* font)
{
    mtx_lock(&font->disp->mutex);
    list_remove(&font->entry);
    mtx_unlock(&font->disp->mutex);
    free(font);
}

int16_t font_kerning_offset(const font_t* font, char firstChar, char secondChar)
{
    if (font == NULL)
    {
        return 0;
    }

    uint32_t offset = font->grf.kernOffsets[(uint8_t)firstChar];
    if (offset == GRF_NONE)
    {
        return 0;
    }

    grf_kern_block_t* block = (grf_kern_block_t*)(&font->grf.buffer[offset]);

    for (uint16_t i = 0; i < block->amount; i++)
    {
        if (block->entries[i].secondChar == (uint8_t)secondChar)
        {
            return block->entries[i].offsetX;
        }

        if (block->entries[i].secondChar > (uint8_t)secondChar)
        {
            break;
        }
    }

    return 0;
}

uint64_t font_width(const font_t* font, const char* string, uint64_t length)
{
    if (string == NULL || length == 0)
    {
        return 0;
    }

    uint64_t width = 0;

    for (uint64_t i = 0; i < length; ++i)
    {
        uint32_t offset = font->grf.glyphOffsets[(uint8_t)string[i]];

        if (offset != GRF_NONE)
        {
            grf_glyph_t* glyph = (grf_glyph_t*)(&font->grf.buffer[offset]);
            width += glyph->advanceX;

            if (i != length - 1)
            {
                width += font_kerning_offset(font, string[i], string[i + 1]);
            }
        }
    }

    return width;
}

uint64_t font_height(const font_t* font)
{
    return font->grf.height;
}
