#include "_AUX/pixel_t.h"
#include "_AUX/rect_t.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/math.h>

#ifndef __EMBED__

fbmp_t* gfx_load_fbmp(const char* path)
{
    fd_t file = open(path);
    if (file == ERR)
    {
        return NULL;
    }

    uint64_t fileSize = seek(file, 0, SEEK_END);
    seek(file, 0, SEEK_SET);

    fbmp_t* image = malloc(fileSize);
    if (image == NULL)
    {
        close(file);
        return NULL;
    }

    if (read(file, image, fileSize) != fileSize)
    {
        close(file);
        free(image);
        return NULL;
    }

    close(file);

    if (image->magic != FBMP_MAGIC)
    {
        free(image);
        return NULL;
    }

    return image;
}

#endif

void gfx_fbmp(surface_t* surface, const fbmp_t* fbmp, const point_t* point)
{
    for (uint32_t y = 0; y < fbmp->height; y++)
    {
        for (uint32_t x = 0; x < fbmp->width; x++)
        {
            surface->buffer[(point->x + x) + (point->y + y) * surface->stride] = fbmp->data[x + y * fbmp->width];
        }
    }

    rect_t rect = RECT_INIT_DIM(point->x, point->x, fbmp->width, fbmp->height);
    gfx_invalidate(surface, &rect);
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
                PIXEL_BLEND(out, &pixel);
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

void gfx_ridge(surface_t* surface, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    gfx_edge(surface, rect, width / 2, background, foreground);

    rect_t innerRect = {
        .left = rect->left + width / 2,
        .top = rect->top + width / 2,
        .right = rect->right - width / 2,
        .bottom = rect->bottom - width / 2,
    };
    gfx_edge(surface, &innerRect, width / 2, foreground, background);
}

void gfx_transfer(surface_t* dest, const surface_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    for (int32_t y = 0; y < RECT_HEIGHT(destRect); y++)
    {
        memcpy(&dest->buffer[destRect->left + (y + destRect->top) * dest->stride],
            &src->buffer[srcPoint->x + (y + srcPoint->y) * src->stride], RECT_WIDTH(destRect) * sizeof(pixel_t));
    }

    gfx_invalidate(dest, destRect);
}

void gfx_transfer_blend(surface_t* dest, const surface_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    for (int32_t y = 0; y < RECT_HEIGHT(destRect); y++)
    {
        for (int32_t x = 0; x < RECT_WIDTH(destRect); x++)
        {
            pixel_t pixel = src->buffer[(srcPoint->x + x) + (srcPoint->y + y) * src->stride];
            pixel_t* out = &dest->buffer[(destRect->left + x) + (destRect->top + y) * dest->stride];
            PIXEL_BLEND(out, &pixel);
        }
    }

    gfx_invalidate(dest, destRect);
}

void gfx_swap(surface_t* dest, const surface_t* src, const rect_t* rect)
{
    for (int32_t y = 0; y < RECT_HEIGHT(rect); y++)
    {
        uint64_t offset = (rect->left * sizeof(pixel_t)) + (y + rect->top) * sizeof(pixel_t) * dest->stride;

        memcpy((void*)((uint64_t)dest->buffer + offset), (void*)((uint64_t)src->buffer + offset),
            RECT_WIDTH(rect) * sizeof(pixel_t));
    }

    gfx_invalidate(dest, rect);
}

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
