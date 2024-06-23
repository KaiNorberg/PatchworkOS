#include <stdint.h>
#include <string.h>
#include <sys/gfx.h>
#include <sys/math.h>

void gfx_invalidate(surface_t* surface, const rect_t* rect)
{
    if (RECT_AREA(&surface->invalidArea) == 0)
    {
        surface->invalidArea = *rect;
    }
    else
    {
        surface->invalidArea.left = MIN(surface->invalidArea.left, rect->left);
        surface->invalidArea.top = MIN(surface->invalidArea.top, rect->top);
        surface->invalidArea.right = MAX(surface->invalidArea.right, rect->right);
        surface->invalidArea.bottom = MAX(surface->invalidArea.bottom, rect->bottom);
    }
}

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

    rect_t rect = (rect_t){
        .left = point->x,
        .top = point->y,
        .right = point->x + PSF_WIDTH * psf->scale,
        .bottom = point->y + PSF_HEIGHT * psf->scale,
    };
    gfx_invalidate(surface, &rect);
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

void gfx_rect(surface_t* surface, const rect_t* rect, pixel_t pixel)
{
    uint64_t pixel64 = ((uint64_t)pixel << 32) | pixel;

    for (int64_t y = rect->top; y < rect->bottom; y++)
    {
        uint64_t count = (rect->right - rect->left) * sizeof(pixel_t);
        uint8_t* ptr = (uint8_t*)&surface->buffer[rect->left + y * surface->stride];

        while (count >= 64)
        {
            *(uint64_t*)(ptr + 0) = pixel64;
            *(uint64_t*)(ptr + 8) = pixel64;
            *(uint64_t*)(ptr + 16) = pixel64;
            *(uint64_t*)(ptr + 24) = pixel64;
            *(uint64_t*)(ptr + 32) = pixel64;
            *(uint64_t*)(ptr + 40) = pixel64;
            *(uint64_t*)(ptr + 48) = pixel64;
            *(uint64_t*)(ptr + 56) = pixel64;
            ptr += 64;
            count -= 64;
        }

        while (count >= 8)
        {
            *(uint64_t*)ptr = pixel64;
            ptr += 8;
            count -= 8;
        }

        while (count--)
        {
            *ptr++ = pixel;
        }
    }

    gfx_invalidate(surface, rect);
}

void gfx_edge(surface_t* surface, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    rect_t leftRect = (rect_t){
        .left = rect->left,
        .top = rect->top,
        .right = rect->left + width,
        .bottom = rect->bottom - width,
    };
    gfx_rect(surface, &leftRect, foreground);

    rect_t topRect = (rect_t){
        .left = rect->left + width,
        .top = rect->top,
        .right = rect->right - width,
        .bottom = rect->top + width,
    };
    gfx_rect(surface, &topRect, foreground);

    rect_t rightRect = (rect_t){
        .left = rect->right - width,
        .top = rect->top + width,
        .right = rect->right,
        .bottom = rect->bottom,
    };
    gfx_rect(surface, &rightRect, background);

    rect_t bottomRect = (rect_t){
        .left = rect->left + width,
        .top = rect->bottom - width,
        .right = rect->right - width,
        .bottom = rect->bottom,
    };
    gfx_rect(surface, &bottomRect, background);

    for (uint64_t y = 0; y < width; y++)
    {
        for (uint64_t x = 0; x < width; x++)
        {
            pixel_t color = x + y < width ? foreground : background;
            surface->buffer[(rect->right - width + x) + (rect->top + y) * surface->stride] = color;
            surface->buffer[(rect->left + x) + (rect->bottom - width + y) * surface->stride] = color;
        }
    }

    gfx_invalidate(surface, rect);
}

void gfx_transfer(surface_t* dest, const surface_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
    {
        uint64_t destOffset = (destRect->left * sizeof(pixel_t)) + (y + destRect->top) * sizeof(pixel_t) * dest->stride;
        uint64_t srcOffset = srcPoint->x * sizeof(pixel_t) + (y + srcPoint->y) * sizeof(pixel_t) * src->stride;

        memcpy((void*)((uint64_t)dest->buffer + destOffset), (void*)((uint64_t)src->buffer + srcOffset),
            RECT_WIDTH(destRect) * sizeof(pixel_t));
    }

    gfx_invalidate(dest, destRect);
}
