#include "_AUX/pixel_t.h"
#include <stdint.h>
#include <sys/gfx.h>

void gfx_psf_char(surface_t* surface, const psf_t* psf, const point_t* point, char chr)
{
    const uint8_t* glyph = psf->glyphs + chr * PSF_HEIGHT;

    if (PIXEL_ALPHA(psf->foreground) == 0xFF && PIXEL_ALPHA(psf->background) == 0xFF)
    {
        for (uint64_t y = 0; y < PSF_HEIGHT * psf->scale; y++)
        {
            for (uint64_t x = 0; x < PSF_WIDTH * psf->scale; x++)
            {
                pixel_t pixel = (*glyph & (0b10000000 >> (x / psf->scale))) > 0 ? psf->foreground : psf->background;
                surface->buffer[(point->x + x) + (point->y + y) * surface->stride] = pixel;
            }
            if (y % psf->scale == 0)
            {
                glyph++;
            }
        }
    }
    else
    {
        for (uint64_t y = 0; y < PSF_HEIGHT * psf->scale; y++)
        {
            for (uint64_t x = 0; x < PSF_WIDTH * psf->scale; x++)
            {
                pixel_t pixel = (*glyph & (0b10000000 >> (x / psf->scale))) > 0 ? psf->foreground : psf->background;
                pixel_t* out = &surface->buffer[(point->x + x) + (point->y + y) * surface->stride];
                *out = PIXEL_BLEND(pixel, *out);
            }
            if (y % psf->scale == 0)
            {
                glyph++;
            }
        }
    }
}

void gfx_psf_string(surface_t* surface, const psf_t* psf, const point_t* point, const char* string)
{
    const char* chr = string;
    uint64_t offset = 0;
    while (*chr != '\0')
    {
        point_t offsetPoint = (point_t){
            .x = point->x + offset,
            .y = point->y,
        };
        gfx_psf_char(surface, psf, &offsetPoint, *chr);
        offset += PSF_WIDTH * psf->scale;
        chr++;
    }
}
