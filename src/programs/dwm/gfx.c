#include "gfx.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/math.h>

#define GFX_VALID_RECT(rect, width, height) \
    ((rect)->left >= 0 && (rect)->top >= 0 && (rect)->right <= (width) && (rect)->bottom <= (height) && \
        (rect)->left < (rect)->right && (rect)->top < (rect)->bottom)

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
