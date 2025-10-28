#include "compositor.h"

#include "dwm.h"
#include "region.h"
#include "screen.h"
#include "surface.h"

#include <stdio.h>

static rect_t screenRect;
static rect_t prevCursorRect;

static region_t invalidRegion = REGION_CREATE;

void compositor_init(void)
{
    screenRect = RECT_INIT_DIM(0, 0, screen_width(), screen_height());
    prevCursorRect = RECT_INIT(0, 0, 0, 0);
}

static bool compositor_draw_surface(surface_t* surface)
{
    if (!(surface->flags & SURFACE_VISIBLE))
    {
        return false;
    }

    region_t surfaceRegion = REGION_CREATE;
    rect_t surfaceRect = SURFACE_SCREEN_RECT(surface);
    region_intersect(&invalidRegion, &surfaceRegion, &surfaceRect);
    if (region_is_empty(&surfaceRegion))
    {
        return false;
    }

    for (uint64_t i = 0; i < surfaceRegion.count; i++)
    {
        screen_transfer(surface, &surfaceRegion.rects[i]);
    }

    region_subtract(&invalidRegion, &surfaceRect);
    return region_is_empty(&invalidRegion);
}

static void compositor_draw_fullscreen(compositor_ctx_t* ctx)
{
    if (!(ctx->fullscreen->flags & SURFACE_VISIBLE))
    {
        return;
    }

    region_t surfaceRegion = REGION_CREATE;
    rect_t surfaceRect = SURFACE_SCREEN_RECT(ctx->fullscreen);
    region_intersect(&invalidRegion, &surfaceRegion, &surfaceRect);
    if (region_is_empty(&surfaceRegion))
    {
        return;
    }

    for (uint64_t i = 0; i < surfaceRegion.count; i++)
    {
        screen_transfer_frontbuffer(ctx->fullscreen, &surfaceRegion.rects[i]);
    }

    region_clear(&invalidRegion);
}

static void compositor_draw_all(compositor_ctx_t* ctx)
{
    if (RECT_AREA(&prevCursorRect) > 0)
    {
        compositor_invalidate(&prevCursorRect);
    }

    if (region_is_empty(&invalidRegion))
    {
        return;
    }

    surface_t* surface;
    LIST_FOR_EACH_REVERSE(surface, ctx->panels, dwmEntry)
    {
        if (compositor_draw_surface(surface))
        {
            goto draw_cursor;
        }
    }

    LIST_FOR_EACH_REVERSE(surface, ctx->windows, dwmEntry)
    {
        if (compositor_draw_surface(surface))
        {
            goto draw_cursor;
        }
    }

    if (ctx->wall != NULL)
    {
        compositor_draw_surface(ctx->wall);
    }

draw_cursor:
    if (ctx->cursor != NULL && (ctx->cursor->flags & SURFACE_VISIBLE))
    {
        rect_t cursorRect = SURFACE_SCREEN_RECT(ctx->cursor);
        screen_transfer_blend(ctx->cursor, &cursorRect);
        prevCursorRect = cursorRect;
    }
    else
    {
        prevCursorRect = RECT_INIT(0, 0, 0, 0);
    }

    region_clear(&invalidRegion);
}

void compositor_draw(compositor_ctx_t* ctx)
{
    if (ctx->wall == NULL)
    {
        return;
    }

    if (ctx->fullscreen != NULL)
    {
        compositor_draw_fullscreen(ctx);
    }
    else
    {
        compositor_draw_all(ctx);
        screen_swap();
    }
}

void compositor_invalidate(const rect_t* rect)
{
    rect_t fitRect = *rect;
    RECT_FIT(&fitRect, &screenRect);
    region_add(&invalidRegion, &fitRect);
}
