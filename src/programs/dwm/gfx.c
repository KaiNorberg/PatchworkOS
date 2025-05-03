#include "gfx.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/math.h>

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

void gfx_fbmp(gfx_t* gfx, const gfx_fbmp_t* fbmp, const point_t* point)
{
    for (int64_t y = 0; y < fbmp->height; y++)
    {
        for (int64_t x = 0; x < fbmp->width; x++)
        {
            gfx->buffer[(point->x + x) + (point->y + y) * gfx->stride] = fbmp->data[x + y * fbmp->width];
        }
    }

    rect_t rect = RECT_INIT_DIM(point->x, point->x, fbmp->width, fbmp->height);
    gfx_invalidate(gfx, &rect);
}

void gfx_fbmp_alpha(gfx_t* gfx, const gfx_fbmp_t* fbmp, const point_t* point)
{
    for (int64_t y = 0; y < fbmp->height; y++)
    {
        for (int64_t x = 0; x < fbmp->width; x++)
        {
            PIXEL_BLEND(&gfx->buffer[(point->x + x) + (point->y + y) * gfx->stride], &fbmp->data[x + y * fbmp->width]);
        }
    }

    rect_t rect = RECT_INIT_DIM(point->x, point->x, fbmp->width, fbmp->height);
    gfx_invalidate(gfx, &rect);
}

void gfx_psf(gfx_t* gfx, const psf_t* psf, const point_t* point, char chr, pixel_t foreground, pixel_t background)
{
    const uint8_t* glyph = psf->glyphs + chr * psf->glyphSize;

    if (PIXEL_ALPHA(foreground) == 0xFF && PIXEL_ALPHA(background) == 0xFF)
    {
        for (uint64_t y = 0; y < psf->height * psf->scale; y++)
        {
            for (uint64_t x = 0; x < psf->width * psf->scale; x++)
            {
                pixel_t pixel = (glyph[y / psf->scale] & (0b10000000 >> (x / psf->scale))) != 0 ? foreground : background;
                gfx->buffer[(point->x + x) + (point->y + y) * gfx->stride] = pixel;
            }
        }
    }
    else
    {
        for (uint64_t y = 0; y < psf->height * psf->scale; y++)
        {
            for (uint64_t x = 0; x < psf->width * psf->scale; x++)
            {
                pixel_t pixel = (glyph[y / psf->scale] & (0b10000000 >> (x / psf->scale))) != 0 ? foreground : background;
                PIXEL_BLEND(&gfx->buffer[(point->x + x) + (point->y + y) * gfx->stride], &pixel);
            }
        }
    }

    rect_t rect = RECT_INIT_DIM(point->x, point->y, psf->width * psf->scale, psf->height * psf->scale);
    gfx_invalidate(gfx, &rect);
}

/*void gfx_text(gfx_t* gfx, const psf_t* psf, const rect_t* rect, gfx_align_t xAlign, gfx_align_t yAlign, uint64_t height,
    const char* str, pixel_t foreground, pixel_t background)
{
    if (str == NULL || *str == '\0')
    {
        return;
    }

    int64_t scale = MAX(1, height / psf->height);
    height = psf->height * scale;
    int64_t width = strlen(str) * psf->width * scale;

    int64_t maxWidth = RECT_WIDTH(rect);
    if (width > maxWidth)
    {
        uint64_t maxLength = maxWidth / (psf->width * scale);
        if (maxLength == 0)
        {
            return;
        }

        const char* dots[] = {".", "..", "..."};
        if (maxLength <= 3)
        {
            gfx_text(gfx, psf, rect, xAlign, yAlign, height, dots[maxLength - 1], foreground, background);
            return;
        }

        char buffer[maxLength + 1];
        memcpy(buffer, str, maxLength - 3);
        memset(buffer + maxLength - 3, '.', 3);
        buffer[maxLength] = '\0';

        gfx_text(gfx, psf, rect, xAlign, yAlign, height, buffer, foreground, background);
        return;
    }

    point_t startPoint;
    switch (xAlign)
    {
    case GFX_CENTER:
    {
        startPoint.x = (rect->left + rect->right) / 2 - width / 2;
    }
    break;
    case GFX_MAX:
    {
        startPoint.x = rect->right - width;
    }
    break;
    case GFX_MIN:
    {
        startPoint.x = rect->left;
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
        startPoint.y = (rect->top + rect->bottom) / 2 - height / 2;
    }
    break;
    case GFX_MAX:
    {
        startPoint.y = MAX(rect->top, rect->bottom - (int64_t)height);
    }
    break;
    case GFX_MIN:
    {
        startPoint.y = rect->top;
    }
    break;
    default:
    {
        return;
    }
    }

    const char* chr = str;
    uint64_t offset = 0;
    while (*chr != '\0')
    {
        point_t point = (point_t){
            .x = startPoint.x + offset,
            .y = startPoint.y,
        };
        gfx_char(gfx, psf, &point, psf->height * scale, *chr, foreground, background);
        offset += psf->width * scale;
        chr++;
    }
}

void gfx_text_multiline(gfx_t* gfx, const psf_t* psf, const rect_t* rect, gfx_align_t xAlign, gfx_align_t yAlign, uint64_t height,
    const char* str, pixel_t foreground, pixel_t background)
{
    if (str == NULL || *str == '\0')
    {
        return;
    }

    int64_t scale = MAX(1, height / psf->height);
    height = psf->height * scale;

    int64_t numLines = 1;
    int64_t maxLineWidth = 0;

    int64_t wordLength = 0;
    int64_t currentXPos = 0;
    const char* chr = str;
    while (true)
    {
        if (*chr == ' ' || *chr == '\0')
        {
            int64_t wordEndPos = currentXPos + wordLength * psf->width * scale;
            if (wordEndPos > RECT_WIDTH(rect))
            {
                numLines++;
                maxLineWidth = MAX(maxLineWidth, currentXPos);
                currentXPos = wordLength * psf->width * scale;
            }
            else
            {
                currentXPos = wordEndPos;
            }
            if (*chr == '\0')
            {
                maxLineWidth = MAX(maxLineWidth, currentXPos);
                break;
            }
            wordLength = 0;
            currentXPos += psf->width * scale;
        }
        else
        {
            wordLength++;
        }
        chr++;
    }

    point_t startPoint;
    switch (xAlign)
    {
    case GFX_CENTER:
    {
        startPoint.x = rect->left + (RECT_WIDTH(rect) - maxLineWidth) / 2;
    }
    break;
    case GFX_MAX:
    {
        startPoint.x = rect->right - maxLineWidth;
    }
    break;
    case GFX_MIN:
    {
        startPoint.x = rect->left;
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
        int64_t totalTextHeight = height * numLines;
        startPoint.y = rect->top + (RECT_HEIGHT(rect) - totalTextHeight) / 2;
    }
    break;
    case GFX_MAX:
    {
        startPoint.y = MAX(rect->top, rect->bottom - (int64_t)height * numLines);
    }
    break;
    case GFX_MIN:
    {
        startPoint.y = rect->top;
    }
    break;
    default:
    {
        return;
    }
    }

    chr = str;
    point_t currentPoint = startPoint;
    while (*chr != '\0')
    {
        const char* wordStart = chr;
        wordLength = 0;
        while (*chr != ' ' && *chr != '\0')
        {
            wordLength++;
            chr++;
        }

        int64_t wordWidth = wordLength * psf->width * scale;

        if (currentPoint.x + wordWidth > rect->right)
        {
            currentPoint.y += height;
            currentPoint.x = startPoint.x;
        }
        for (int64_t i = 0; i < wordLength; i++)
        {
            gfx_char(gfx, psf, &currentPoint, height, wordStart[i], foreground, background);
            currentPoint.x += psf->width * scale;
        }

        if (*chr == ' ')
        {
            if (currentPoint.x + psf->width * scale > rect->right)
            {
                currentPoint.y += height;
            }
            else
            {
                gfx_char(gfx, psf, &currentPoint, height, ' ', foreground, background);
                currentPoint.x += psf->width * scale;
            }
            chr++;
        }
    }
}*/

void gfx_rect(gfx_t* gfx, const rect_t* rect, pixel_t pixel)
{
    if (rect->left < 0 || rect->top < 0 || rect->right > gfx->width || rect->bottom > gfx->height)
    {
        return;
    }

    uint64_t pixel64 = ((uint64_t)pixel << 32) | pixel;

    for (int64_t y = rect->top; y < rect->bottom; y++)
    {
        uint8_t* ptr = (uint8_t*)&gfx->buffer[rect->left + y * gfx->stride];
        uint64_t count = (rect->right - rect->left) * sizeof(pixel_t);

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

        while (count >= sizeof(pixel_t))
        {
            *(pixel_t*)ptr = pixel;
            ptr += sizeof(pixel_t);
            count -= sizeof(pixel_t);
        }
    }

    gfx_invalidate(gfx, rect);
}

void gfx_gradient(gfx_t* gfx, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type, bool addNoise)
{
    int64_t width = rect->right - rect->left;
    int64_t height = rect->bottom - rect->top;

    for (int64_t y = rect->top; y < rect->bottom; y++)
    {
        for (int64_t x = rect->left; x < rect->right; x++)
        {
            int32_t factorNum;
            int32_t factorDenom;

            switch (type)
            {
            case GRADIENT_VERTICAL:
            {
                factorNum = (y - rect->top);
                factorDenom = height;
            }
            break;
            case GRADIENT_HORIZONTAL:
            {
                factorNum = (x - rect->left);
                factorDenom = width;
            }
            break;
            case GRADIENT_DIAGONAL:
            {
                factorNum = (x - rect->left) + (y - rect->top);
                factorDenom = width + height;
            }
            break;
            default:
            {
                factorNum = 0;
                factorDenom = 1;
            }
            break;
            }

            int32_t deltaRed = PIXEL_RED(end) - PIXEL_RED(start);
            int32_t deltaGreen = PIXEL_GREEN(end) - PIXEL_GREEN(start);
            int32_t deltaBlue = PIXEL_BLUE(end) - PIXEL_BLUE(start);

            int32_t red = PIXEL_RED(start) + ((factorNum * deltaRed) / factorDenom);
            int32_t green = PIXEL_GREEN(start) + ((factorNum * deltaGreen) / factorDenom);
            int32_t blue = PIXEL_BLUE(start) + ((factorNum * deltaBlue) / factorDenom);

            if (addNoise)
            {
                int32_t noiseRed = (rand() % 5) - 2;
                int32_t noiseGreen = (rand() % 5) - 2;
                int32_t noiseBlue = (rand() % 5) - 2;

                red += noiseRed;
                green += noiseGreen;
                blue += noiseBlue;

                red = CLAMP(0, 255, red);
                green = CLAMP(0, 255, green);
                blue = CLAMP(0, 255, blue);
            }

            pixel_t pixel = PIXEL_ARGB(255, red, green, blue);
            gfx->buffer[x + y * gfx->stride] = pixel;
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
    int64_t width = RECT_WIDTH(destRect);
    int64_t height = RECT_HEIGHT(destRect);

    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (srcPoint->x < 0 || srcPoint->y < 0 || srcPoint->x + width > src->width || srcPoint->y + height > src->height)
    {
        return;
    }
    if (destRect->left < 0 || destRect->top < 0 || destRect->left + width > dest->width || destRect->top + height > dest->height)
    {
        return;
    }

    for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
    {
        memcpy(&dest->buffer[destRect->left + (y + destRect->top) * dest->stride],
            &src->buffer[srcPoint->x + (y + srcPoint->y) * src->stride], RECT_WIDTH(destRect) * sizeof(pixel_t));
    }

    gfx_invalidate(dest, destRect);
}

void gfx_transfer_blend(gfx_t* dest, const gfx_t* src, const rect_t* destRect, const point_t* srcPoint)
{
    int64_t width = RECT_WIDTH(destRect);
    int64_t height = RECT_HEIGHT(destRect);

    if (width <= 0 || height <= 0)
    {
        return;
    }
    if (srcPoint->x < 0 || srcPoint->y < 0 || srcPoint->x + width > src->width || srcPoint->y + height > src->height)
    {
        return;
    }
    if (destRect->left < 0 || destRect->top < 0 || destRect->left + width > dest->width || destRect->top + height > dest->height)
    {
        return;
    }

    for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
    {
        for (int64_t x = 0; x < RECT_WIDTH(destRect); x++)
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
    for (int64_t y = 0; y < RECT_HEIGHT(rect); y++)
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
        RECT_EXPAND_TO_CONTAIN(&gfx->invalidRect, rect);
    }
}
