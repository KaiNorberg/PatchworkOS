#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/gfx.h>
#include <sys/io.h>
#include <sys/math.h>

#include "platform/platform.h"
#if _PLATFORM_HAS_FILE_IO

gfx_fbmp_t* gfx_fbmp_new(const char* path)
{
    fd_t file = open(path);
    if (file == ERR)
    {
        return NULL;
    }

    uint64_t fileSize = seek(file, 0, SEEK_END);
    seek(file, 0, SEEK_SET);

    gfx_fbmp_t* image = malloc(fileSize);
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

static gfx_psf_t* gfx_psf1_load(fd_t file)
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

    gfx_psf_t* psf = malloc(sizeof(gfx_psf_t) + glyphBufferSize);
    if (psf == NULL)
    {
        return NULL;
    }

    psf->width = 8;
    psf->height = header.glyphSize;
    psf->glyphSize = header.glyphSize;
    psf->glyphAmount = glyphAmount;
    if (read(file, psf->glyphs, glyphBufferSize) != glyphBufferSize)
    {
        free(psf);
        return NULL;
    }

    return psf;
}

static gfx_psf_t* gfx_psf2_load(fd_t file)
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

    gfx_psf_t* psf = malloc(sizeof(gfx_psf_t) + glyphBufferSize);
    if (psf == NULL)
    {
        return NULL;
    }

    psf->width = header.width;
    psf->height = header.height;
    psf->glyphSize = header.glyphSize;
    psf->glyphAmount = header.glyphAmount;
    if (read(file, psf->glyphs, glyphBufferSize) != glyphBufferSize)
    {
        free(psf);
        return NULL;
    }

    return psf;
}

gfx_psf_t* gfx_psf_new(const char* path)
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

    gfx_psf_t* psf;
    if (firstByte == 0x36) // Is psf1
    {
        psf = gfx_psf1_load(file);
    }
    else if (firstByte == 0x72) // Is psf2
    {
        psf = gfx_psf2_load(file);
    }
    else
    {
        psf = NULL;
    }

    close(file);
    return psf;
}

#endif

void gfx_fbmp(gfx_t* gfx, const gfx_fbmp_t* fbmp, const point_t* point)
{
    for (uint32_t y = 0; y < fbmp->height; y++)
    {
        for (uint32_t x = 0; x < fbmp->width; x++)
        {
            gfx->buffer[(point->x + x) + (point->y + y) * gfx->stride] = fbmp->data[x + y * fbmp->width];
        }
    }

    rect_t rect = RECT_INIT_DIM(point->x, point->x, fbmp->width, fbmp->height);
    gfx_invalidate(gfx, &rect);
}

void gfx_char(gfx_t* gfx, const gfx_psf_t* psf, const point_t* point, uint64_t height, char chr, pixel_t foreground,
    pixel_t background)
{
    uint64_t scale = MAX(1, height / psf->height);
    const uint8_t* glyph = psf->glyphs + chr * psf->glyphSize;

    for (uint64_t y = 0; y < psf->height * scale; y++)
    {
        for (uint64_t x = 0; x < psf->width * scale; x++)
        {
            pixel_t pixel = (glyph[y / scale] & (0b10000000 >> (x / scale))) != 0 ? foreground : background;
            PIXEL_BLEND(&gfx->buffer[(point->x + x) + (point->y + y) * gfx->stride], &pixel);
        }
    }

    rect_t rect = RECT_INIT_DIM(point->x, point->y, psf->width * scale, psf->height * scale);
    gfx_invalidate(gfx, &rect);
}

void gfx_text(gfx_t* gfx, const gfx_psf_t* psf, const rect_t* rect, gfx_align_t xAlign, gfx_align_t yAlign, uint64_t height,
    const char* str, pixel_t foreground, pixel_t background)
{
    uint64_t scale = MAX(1, height / psf->height);
    height = psf->height * scale;
    int64_t width = strlen(str) * psf->width * scale;

    point_t point;
    switch (xAlign)
    {
    case GFX_CENTER:
    {
        point.x = MAX(rect->left, (rect->right + rect->left - width) / 2);
    }
    break;
    case GFX_MAX:
    {
        point.x = MAX(rect->left, rect->right - width);
    }
    break;
    case GFX_MIN:
    {
        point.x = rect->left;
    }
    break;
    default:
    {
        return;
    }
    }

    switch (yAlign)
    {
    case GFX_CENTER:
    {
        point.y = MAX(rect->top, (rect->bottom + rect->top - (int64_t)height) / 2);
    }
    break;
    case GFX_MAX:
    {
        point.y = MAX(rect->top, rect->bottom - (int64_t)height);
    }
    break;
    case GFX_MIN:
    {
        point.y = rect->top;
    }
    break;
    default:
    {
        return;
    }
    }

    if (RECT_WIDTH(rect) < width)
    {
        for (uint64_t i = 0; i < RECT_WIDTH(rect) / (psf->width * scale) - 3; i++)
        {
            gfx_char(gfx, psf, &point, height, str[i], foreground, background);
            point.x += psf->width * scale;
        }

        for (uint64_t i = 0; i < 3; i++)
        {
            gfx_char(gfx, psf, &point, height, '.', foreground, background);
            point.x += psf->width * scale;
        }
    }
    else
    {
        const char* chr = str;
        uint64_t offset = 0;
        while (*chr != '\0')
        {
            gfx_char(gfx, psf, &point, height, *chr, foreground, background);
            point.x += psf->width * scale;
            chr++;
        }
    }
}

void gfx_rect(gfx_t* gfx, const rect_t* rect, pixel_t pixel)
{
    uint64_t pixel64 = ((uint64_t)pixel << 32) | pixel;

    for (int64_t y = rect->top; y < rect->bottom; y++)
    {
        uint64_t count = (rect->right - rect->left) * sizeof(pixel_t);
        uint8_t* ptr = (uint8_t*)&gfx->buffer[rect->left + y * gfx->stride];

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

    gfx_invalidate(gfx, rect);
}

void gfx_edge(gfx_t* gfx, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    rect_t leftRect = (rect_t){
        .left = rect->left,
        .top = rect->top,
        .right = rect->left + width,
        .bottom = rect->bottom - width,
    };
    gfx_rect(gfx, &leftRect, foreground);

    rect_t topRect = (rect_t){
        .left = rect->left + width,
        .top = rect->top,
        .right = rect->right - width,
        .bottom = rect->top + width,
    };
    gfx_rect(gfx, &topRect, foreground);

    rect_t rightRect = (rect_t){
        .left = rect->right - width,
        .top = rect->top + width,
        .right = rect->right,
        .bottom = rect->bottom,
    };
    gfx_rect(gfx, &rightRect, background);

    rect_t bottomRect = (rect_t){
        .left = rect->left + width,
        .top = rect->bottom - width,
        .right = rect->right - width,
        .bottom = rect->bottom,
    };
    gfx_rect(gfx, &bottomRect, background);

    for (uint64_t y = 0; y < width; y++)
    {
        for (uint64_t x = 0; x < width; x++)
        {
            pixel_t color = x + y < width - 1 ? foreground : background;
            gfx->buffer[(rect->right - width + x) + (rect->top + y) * gfx->stride] = color;
            gfx->buffer[(rect->left + x) + (rect->bottom - width + y) * gfx->stride] = color;
        }
    }

    gfx_invalidate(gfx, rect);
}

void gfx_ridge(gfx_t* gfx, const rect_t* rect, uint64_t width, pixel_t foreground, pixel_t background)
{
    gfx_edge(gfx, rect, width / 2, background, foreground);

    rect_t innerRect = *rect;
    RECT_SHRINK(&innerRect, width / 2);
    gfx_edge(gfx, &innerRect, width / 2, foreground, background);
}

void gfx_scroll(gfx_t* gfx, const rect_t* rect, uint64_t offset, pixel_t background)
{
    int64_t width = RECT_WIDTH(rect);
    int64_t height = RECT_HEIGHT(rect);

    for (uint64_t y = 0; y < height - offset; y++)
    {
        pixel_t* src = &gfx->buffer[rect->left + (rect->top + y + offset) * gfx->stride];
        pixel_t* dest = &gfx->buffer[rect->left + (rect->top + y) * gfx->stride];
        memmove(dest, src, width * sizeof(pixel_t));
    }

    for (int64_t y = height - offset; y < height; y++)
    {
        pixel_t* dest = &gfx->buffer[rect->left + (rect->top + y) * gfx->stride];
        for (int64_t x = 0; x < width; x++)
        {
            dest[x] = background;
        }
    }

    gfx_invalidate(gfx, rect);
}

void gfx_rim(gfx_t* gfx, const rect_t* rect, uint64_t width, pixel_t pixel)
{
    rect_t leftRect = (rect_t){
        .left = rect->left,
        .top = rect->top + width - width / 2,
        .right = rect->left + width,
        .bottom = rect->bottom - width + width / 2,
    };
    gfx_rect(gfx, &leftRect, pixel);

    rect_t topRect = (rect_t){
        .left = rect->left + width - width / 2,
        .top = rect->top,
        .right = rect->right - width + width / 2,
        .bottom = rect->top + width,
    };
    gfx_rect(gfx, &topRect, pixel);

    rect_t rightRect = (rect_t){
        .left = rect->right - width,
        .top = rect->top + width - width / 2,
        .right = rect->right,
        .bottom = rect->bottom - width + width / 2,
    };
    gfx_rect(gfx, &rightRect, pixel);

    rect_t bottomRect = (rect_t){
        .left = rect->left + width - width / 2,
        .top = rect->bottom - width,
        .right = rect->right - width + width / 2,
        .bottom = rect->bottom,
    };
    gfx_rect(gfx, &bottomRect, pixel);
}

void gfx_transfer(gfx_t* dest, const gfx_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    for (int32_t y = 0; y < RECT_HEIGHT(destRect); y++)
    {
        memcpy(&dest->buffer[destRect->left + (y + destRect->top) * dest->stride],
            &src->buffer[srcPoint->x + (y + srcPoint->y) * src->stride], RECT_WIDTH(destRect) * sizeof(pixel_t));
    }

    gfx_invalidate(dest, destRect);
}

void gfx_transfer_blend(gfx_t* dest, const gfx_t* src, const rect_t* destRect, const point_t* srcPoint)
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

void gfx_swap(gfx_t* dest, const gfx_t* src, const rect_t* rect)
{
    for (int32_t y = 0; y < RECT_HEIGHT(rect); y++)
    {
        uint64_t offset = (rect->left * sizeof(pixel_t)) + (y + rect->top) * sizeof(pixel_t) * dest->stride;

        memcpy((void*)((uint64_t)dest->buffer + offset), (void*)((uint64_t)src->buffer + offset),
            RECT_WIDTH(rect) * sizeof(pixel_t));
    }

    gfx_invalidate(dest, rect);
}

void gfx_invalidate(gfx_t* gfx, const rect_t* rect)
{
    if (RECT_AREA(&gfx->invalidRect) == 0)
    {
        gfx->invalidRect = *rect;
    }
    else
    {
        gfx->invalidRect.left = MIN(gfx->invalidRect.left, rect->left);
        gfx->invalidRect.top = MIN(gfx->invalidRect.top, rect->top);
        gfx->invalidRect.right = MAX(gfx->invalidRect.right, rect->right);
        gfx->invalidRect.bottom = MAX(gfx->invalidRect.bottom, rect->bottom);
    }
}
