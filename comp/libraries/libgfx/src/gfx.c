#include <float.h>
#include <libc/cpuid.h>
#include <libc/math.h>
#include <libc/proc.h>
#include <libgfx/gfx.h>
#include <libgfx/poly.h>
#include <stdlib.h>
#include <string.h>

#include "poly_internal.h"

void gfx_draw_rect(gfx_t* draw, gfx_rect_t rect, gfx_pixel_t color)
{
    uint64_t width = GFX_RECT_WIDTH(rect);
    if (width * sizeof(gfx_pixel_t) == draw->pitch)
    {
        memset32(GFX_GET_PIXEL(draw, rect.left, rect.top), color.argb, width * GFX_RECT_HEIGHT(rect));
        return;
    }

    for (int32_t y = rect.top; y < rect.bottom; y++)
    {
        gfx_pixel_t* row = GFX_GET_PIXEL(draw, rect.left, y);
        memset32(row, color.argb, width);
    }
}

void gfx_draw_copy(gfx_t* dst, gfx_t* src, gfx_rect_t dstRect, gfx_rect_t srcRect)
{
    int32_t width = MIN(GFX_RECT_WIDTH(dstRect), GFX_RECT_WIDTH(srcRect));
    int32_t height = MIN(GFX_RECT_HEIGHT(dstRect), GFX_RECT_HEIGHT(srcRect));

    if (dst->pitch == src->pitch && (uint32_t)width * sizeof(gfx_pixel_t) == dst->pitch)
    {
        memcpy(GFX_GET_PIXEL(dst, dstRect.left, dstRect.top), GFX_GET_PIXEL(src, srcRect.left, srcRect.top),
            (size_t)width * height * sizeof(gfx_pixel_t));
        return;
    }

    for (int32_t y = 0; y < height; y++)
    {
        gfx_pixel_t* dstRow = GFX_GET_PIXEL(dst, dstRect.left, dstRect.top + y);
        gfx_pixel_t* srcRow = GFX_GET_PIXEL(src, srcRect.left, srcRect.top + y);
        memcpy(dstRow, srcRow, (size_t)width * sizeof(gfx_pixel_t));
    }
}

#define DIV_255(x) \
    ({ \
        typeof(x) _x = (x); \
        (((_x) + 1 + ((_x) >> 8)) >> 8); \
    })

static void gfx_draw_blit_no_simd(gfx_pixel_t* dst, gfx_pixel_t* src, int32_t width, int32_t height, int32_t dstPitch,
    int32_t srcPitch)
{
    /// @todo Consider implementing SIMD blit, even though the compiler seems to be smarter than me so ive been unable
    /// to make something faster than what it turns the `gfx_draw_blit_no_simd()` function into.

    for (int32_t y = 0; y < height; y++)
    {
        gfx_pixel_t* srcRow = src;
        gfx_pixel_t* dstRow = dst;

        for (int32_t x = 0; x < width; x++)
        {
            gfx_pixel_t s = srcRow[x];
            uint8_t alpha = s.a;

            if (alpha == 0)
            {
                continue;
            }

            if (alpha == 255)
            {
                dstRow[x] = s;
                continue;
            }

            gfx_pixel_t d = dstRow[x];
            uint8_t invAlpha = 255 - alpha;

            dstRow[x].r = (uint8_t)DIV_255(s.r * alpha + d.r * invAlpha);
            dstRow[x].g = (uint8_t)DIV_255(s.g * alpha + d.g * invAlpha);
            dstRow[x].b = (uint8_t)DIV_255(s.b * alpha + d.b * invAlpha);
            dstRow[x].a = 255;
        }

        dst = (gfx_pixel_t*)((uint8_t*)dst + dstPitch);
        src = (gfx_pixel_t*)((uint8_t*)src + srcPitch);
    }
}

static void gfx_draw_blit_select(gfx_pixel_t* dst, gfx_pixel_t* src, int32_t width, int32_t height, int32_t dstPitch,
    int32_t srcPitch);
static void (*gfx_draw_blit_impl)(gfx_pixel_t*, gfx_pixel_t*, int32_t, int32_t, int32_t, int32_t) = gfx_draw_blit_select;

static void gfx_draw_blit_select(gfx_pixel_t* dst, gfx_pixel_t* src, int32_t width, int32_t height, int32_t dstPitch,
    int32_t srcPitch)
{
    cpuid_instruction_sets_t sets = cpuid_detect_instruction_sets();

    gfx_draw_blit_impl = gfx_draw_blit_no_simd;

    gfx_draw_blit_impl(dst, src, width, height, dstPitch, srcPitch);
}

void gfx_draw_blit(gfx_t* dst, gfx_t* src, gfx_rect_t dstRect, gfx_rect_t srcRect)
{
    gfx_draw_blit_impl(GFX_GET_PIXEL(dst, dstRect.left, dstRect.top), GFX_GET_PIXEL(src, srcRect.left, srcRect.top),
        MIN(GFX_RECT_WIDTH(dstRect), GFX_RECT_WIDTH(srcRect)), MIN(GFX_RECT_HEIGHT(dstRect), GFX_RECT_HEIGHT(srcRect)), dst->pitch,
        src->pitch);
}

static inline uint64_t edge_get_left_mask(const gfx_poly_edge_t* e, int px)
{
    uint64_t mask = 0;
    float base = 8.0f * (e->x - (float)px) - 0.5f;
    for (int32_t sy = 0; sy < 8; sy++)
    {
        float val = base + (sy + 0.5f) * e->invSlope;
        int32_t k = (int32_t)ceilf(val);
        if (k < 0)
        {
            k = 0;
        }

        if (k < 8)
        {
            uint64_t rowMask = (0xFFU << k) & 0xFFU;
            mask |= (rowMask << (sy * 8));
        }
    }
    return mask;
}

static inline uint64_t edge_get_right_mask(const gfx_poly_edge_t* e, int px)
{
    uint64_t mask = 0;
    float base = 8.0f * (e->x - (float)px) - 0.5f;
    for (int32_t sy = 0; sy < 8; sy++)
    {
        float val = base + (sy + 0.5f) * e->invSlope;
        int32_t k = (int32_t)floorf(val);
        if (k > 7)
        {
            k = 7;
        }

        if (k >= 0)
        {
            uint64_t rowMask = (1U << (k + 1)) - 1;
            mask |= (rowMask << (sy * 8));
        }
    }
    return mask;
}

static inline void gfx_draw_gfx_poly_pixel(gfx_t* draw, int32_t x, int32_t y, gfx_pixel_t color, uint8_t alpha,
    gfx_draw_blend_t blend)
{
    gfx_pixel_t* pixel = GFX_GET_PIXEL(draw, x, y);

    if (alpha == 255 && (blend == GFX_BLEND_NONE || blend == GFX_BLEND_ALPHA || blend == GFX_BLEND_SET))
    {
        *pixel = color;
    }
    else if (blend == GFX_BLEND_NONE || blend == GFX_BLEND_ALPHA)
    {
        uint8_t invAlpha = 255 - alpha;
        pixel->r = DIV_255((color.r * alpha + pixel->r * invAlpha));
        pixel->g = DIV_255((color.g * alpha + pixel->g * invAlpha));
        pixel->b = DIV_255((color.b * alpha + pixel->b * invAlpha));
        pixel->a = 255;
    }
    else if (blend == GFX_BLEND_ADDITIVE)
    {
        pixel->r = MIN(255, DIV_255((int32_t)pixel->r + (color.r * alpha)));
        pixel->g = MIN(255, DIV_255((int32_t)pixel->g + (color.g * alpha)));
        pixel->b = MIN(255, DIV_255((int32_t)pixel->b + (color.b * alpha)));
        pixel->a = 255;
    }
    else if (blend == GFX_BLEND_MULTIPLY)
    {
        uint8_t invAlpha = 255 - alpha;
        uint8_t rM = DIV_255(pixel->r * color.r);
        uint8_t gM = DIV_255(pixel->g * color.g);
        uint8_t bM = DIV_255(pixel->b * color.b);

        pixel->r = DIV_255(rM * alpha + pixel->r * invAlpha);
        pixel->g = DIV_255(gM * alpha + pixel->g * invAlpha);
        pixel->b = DIV_255(bM * alpha + pixel->b * invAlpha);
        pixel->a = 255;
    }
    else if (blend == GFX_BLEND_SET)
    {
        pixel->r = color.r;
        pixel->g = color.g;
        pixel->b = color.b;
        pixel->a = alpha;
    }
}

static inline void gfx_draw_gfx_poly_sort_active_edges(gfx_poly_edge_t** edges, size_t count)
{
    // The active edges array is usually very small so insertion sort is faster than quick sort.
    if (count < 2)
    {
        return;
    }

    for (size_t i = 1; i < count; i++)
    {
        gfx_poly_edge_t* key = edges[i];
        intptr_t j = (intptr_t)i - 1;
        while (j >= 0 && edges[j]->x > key->x)
        {
            edges[j + 1] = edges[j];
            j--;
        }
        edges[j + 1] = key;
    }
}

void gfx_draw_polygon(gfx_t* draw, gfx_poly_t* polygon, gfx_pixel_t color, gfx_draw_blend_t blend)
{
    if (draw == NULL || polygon == NULL)
    {
        return;
    }

    uint8_t baseAlpha = (blend == GFX_BLEND_NONE) ? 255 : color.a;

    size_t activeEdgeCount = 0;
    gfx_poly_edge_t* nextEdge = &polygon->edges[0];
    for (int32_t scanline = polygon->top; scanline <= polygon->bottom; scanline++)
    {
        while (nextEdge < &polygon->edges[polygon->edgeCount] && nextEdge->top < ((float)scanline + 1.0F))
        {
            nextEdge->x = nextEdge->start + (((float)scanline - nextEdge->top) * nextEdge->invSlope);
            polygon->activeEdges[activeEdgeCount++] = nextEdge++;
        }

        for (size_t i = 0; i < activeEdgeCount; i++)
        {
            if (polygon->activeEdges[i]->bottom <= (float)scanline)
            {
                polygon->activeEdges[i] = polygon->activeEdges[--activeEdgeCount];
                i--;
            }
        }

        if (activeEdgeCount < 2)
        {
            continue;
        }

        if (scanline >= (int32_t)draw->height)
        {
            break;
        }

        gfx_draw_gfx_poly_sort_active_edges(polygon->activeEdges, activeEdgeCount);

        if (scanline < 0)
        {
            goto update_edges;
        }

        int32_t winding = 0;
        gfx_poly_edge_t* leftEdge = NULL;

        for (uint64_t i = 0; i < activeEdgeCount; i++)
        {
            gfx_poly_edge_t* edge = polygon->activeEdges[i];
            int32_t prevWinding = winding;
            winding += edge->dir;

            if (prevWinding == 0 && winding != 0)
            {
                leftEdge = edge;
                continue;
            }

            if (prevWinding == 0 || winding != 0)
            {
                continue;
            }

            gfx_poly_edge_t* rightEdge = edge;

            if (leftEdge == NULL)
            {
                continue;
            }

            int32_t start = (int32_t)floorf(MIN(leftEdge->x, leftEdge->x + leftEdge->invSlope));
            int32_t end = (int32_t)ceilf(MAX(rightEdge->x, rightEdge->x + rightEdge->invSlope));

            start = MAX(start, 0);
            end = MIN(end, (int32_t)draw->width - 1);

            int32_t leftStart = (int32_t)floorf(MIN(leftEdge->x, leftEdge->x + leftEdge->invSlope)) - 1;
            int32_t leftEnd = (int32_t)ceilf(MAX(leftEdge->x, leftEdge->x + leftEdge->invSlope)) + 1;

            int32_t rightStart = (int32_t)floorf(MIN(rightEdge->x, rightEdge->x + rightEdge->invSlope)) - 1;
            int32_t rightEnd = (int32_t)ceilf(MAX(rightEdge->x, rightEdge->x + rightEdge->invSlope)) + 1;

            int32_t edge1Start = MAX(start, leftStart);
            int32_t edge1End = MIN(end, leftEnd);

            int32_t innerStart = MAX(start, leftEnd + 1);
            int32_t innerEnd = MIN(end, rightStart - 1);

            int32_t edge2Start = MAX(start, rightStart);
            int32_t edge2End = MIN(end, rightEnd);

            for (int32_t x = edge1Start; x <= edge1End; x++)
            {
                uint64_t mask = ~0ULL;

                if (x >= leftStart && x <= leftEnd)
                {
                    mask &= edge_get_left_mask(leftEdge, x);
                }

                if (mask == 0)
                {
                    continue;
                }

                int32_t coverage = count_set_bits(mask);
                uint8_t alpha = (uint8_t)((baseAlpha * coverage) / 64);

                if (alpha == 0)
                {
                    continue;
                }

                gfx_draw_gfx_poly_pixel(draw, x, scanline, color, alpha, blend);
            }

            if (innerStart <= innerEnd)
            {
                if (blend == GFX_BLEND_NONE || blend == GFX_BLEND_SET ||
                    (blend == GFX_BLEND_ALPHA && baseAlpha == 255))
                {
                    int32_t width = innerEnd - innerStart + 1;
                    memset32(GFX_GET_PIXEL(draw, innerStart, scanline), color.argb, width);
                }
                else
                {
                    for (int32_t x = innerStart; x <= innerEnd; x++)
                    {
                        gfx_draw_gfx_poly_pixel(draw, x, scanline, color, baseAlpha, blend);
                    }
                }
            }

            for (int32_t x = edge2Start; x <= edge2End; x++)
            {
                uint64_t mask = ~0ULL;

                if (x >= rightStart && x <= rightEnd)
                {
                    mask &= edge_get_right_mask(rightEdge, x);
                }

                if (mask == 0)
                {
                    continue;
                }

                int32_t coverage = count_set_bits(mask);
                uint8_t alpha = (uint8_t)((baseAlpha * coverage) / 64);

                if (alpha == 0)
                {
                    continue;
                }

                gfx_draw_gfx_poly_pixel(draw, x, scanline, color, alpha, blend);
            }
        }

update_edges:
        for (uint64_t i = 0; i < activeEdgeCount; i++)
        {
            polygon->activeEdges[i]->x += polygon->activeEdges[i]->invSlope;
        }
    }
}