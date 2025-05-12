#include "gfx.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/math.h>

#define GFX_VALID_RECT(rect, width, height) \
    ((rect)->left >= 0 && (rect)->top >= 0 && (rect)->right <= (width) && (rect)->bottom <= (height) && \
        (rect)->left < (rect)->right && (rect)->top < (rect)->bottom)

void gfx_psf(gfx_t* gfx, const psf_t* psf, const point_t* point, char chr, pixel_t foreground, pixel_t background)
{
    if (psf->glyphAmount < (uint32_t)chr)
    {

        return;
    }

    const uint8_t* glyph = psf->glyphs + chr * psf->glyphSize;

    if (PIXEL_ALPHA(foreground) == 0xFF && PIXEL_ALPHA(background) == 0xFF)
    {
        for (uint64_t y = 0; y < psf->height * psf->scale; y++)
        {
            for (uint64_t x = 0; x < psf->width * psf->scale; x++)
            {
                pixel_t pixel =
                    (glyph[y / psf->scale] & (0b10000000 >> (x / psf->scale))) != 0 ? foreground : background;
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
                pixel_t pixel =
                    (glyph[y / psf->scale] & (0b10000000 >> (x / psf->scale))) != 0 ? foreground : background;
                PIXEL_BLEND(&gfx->buffer[(point->x + x) + (point->y + y) * gfx->stride], &pixel);
            }
        }
    }

    rect_t rect = RECT_INIT_DIM(point->x, point->y, psf->width * psf->scale, psf->height * psf->scale);
    gfx_invalidate(gfx, &rect);
}

void gfx_rect(gfx_t* gfx, const rect_t* rect, pixel_t pixel)
{
    if (!GFX_VALID_RECT(rect, gfx->width, gfx->height))
    {
        return;
    }

    for (int64_t y = rect->top; y < rect->bottom; y++)
    {
        memset32(&gfx->buffer[rect->left + y * gfx->stride], pixel, RECT_WIDTH(rect));
    }

    gfx_invalidate(gfx, rect);
}

void gfx_gradient(gfx_t* gfx, const rect_t* rect, pixel_t start, pixel_t end, gradient_type_t type, bool addNoise)
{
    if (!GFX_VALID_RECT(rect, gfx->width, gfx->height))
    {
        return;
    }

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
    if (!GFX_VALID_RECT(rect, gfx->width, gfx->height))
    {
        return;
    }

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
    if (!GFX_VALID_RECT(rect, gfx->width, gfx->height))
    {
        return;
    }

    gfx_edge(gfx, rect, width / 2, background, foreground);

    rect_t innerRect = *rect;
    RECT_SHRINK(&innerRect, width / 2);
    gfx_edge(gfx, &innerRect, width / 2, foreground, background);
}

void gfx_scroll(gfx_t* gfx, const rect_t* rect, uint64_t offset, pixel_t background)
{
    if (!GFX_VALID_RECT(rect, gfx->width, gfx->height))
    {
        return;
    }

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
        memset32(dest, background, width);
    }

    gfx_invalidate(gfx, rect);
}

void gfx_rim(gfx_t* gfx, const rect_t* rect, uint64_t width, pixel_t pixel)
{
    if (!GFX_VALID_RECT(rect, gfx->width, gfx->height))
    {
        return;
    }

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
    if (destRect->left < 0 || destRect->top < 0 || destRect->left + width > dest->width ||
        destRect->top + height > dest->height)
    {
        return;
    }

    if (dest == src)
    {
        for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
        {
            memmove(&dest->buffer[destRect->left + (y + destRect->top) * dest->stride],
                &src->buffer[srcPoint->x + (y + srcPoint->y) * src->stride], RECT_WIDTH(destRect) * sizeof(pixel_t));
        }
    }
    else
    {
        for (int64_t y = 0; y < RECT_HEIGHT(destRect); y++)
        {
            memcpy(&dest->buffer[destRect->left + (y + destRect->top) * dest->stride],
                &src->buffer[srcPoint->x + (y + srcPoint->y) * src->stride], RECT_WIDTH(destRect) * sizeof(pixel_t));
        }
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
    if (destRect->left < 0 || destRect->top < 0 || destRect->left + width > dest->width ||
        destRect->top + height > dest->height)
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
