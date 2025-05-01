#include "compositor.h"

#include "dwm.h"
#include "screen.h"

#include <stdio.h>

static bool redrawNeeded;

static rect_t screenRect;
static rect_t clientRect;

void compositor_init(void)
{
    redrawNeeded = false;
    screenRect = RECT_INIT_DIM(0, 0, screen_width(), screen_height());
    clientRect = screenRect;
}

static void compositor_compute_client_area(compositor_ctx_t* ctx)
{
    clientRect = screenRect;

    surface_t* panel;
    LIST_FOR_EACH(panel, ctx->panels, dwmEntry)
    {
        uint64_t leftDist = panel->pos.x + panel->gfx.width;
        uint64_t topDist = panel->pos.y + panel->gfx.height;
        uint64_t rightDist = RECT_WIDTH(&screenRect) - panel->pos.x;
        uint64_t bottomDist = RECT_HEIGHT(&screenRect) - panel->pos.y;

        if (leftDist <= topDist && leftDist <= rightDist && leftDist <= bottomDist)
        {
            clientRect.left = MAX(panel->pos.x + panel->gfx.width, clientRect.left);
        }
        else if (topDist <= leftDist && topDist <= rightDist && topDist <= bottomDist)
        {
            clientRect.top = MAX(panel->pos.y + panel->gfx.height, clientRect.top);
        }
        else if (rightDist <= leftDist && rightDist <= topDist && rightDist <= bottomDist)
        {
            clientRect.right = MIN(panel->pos.x, clientRect.right);
        }
        else if (bottomDist <= leftDist && bottomDist <= topDist && bottomDist <= rightDist)
        {
            clientRect.bottom = MIN(panel->pos.y, clientRect.bottom);
        }
    }
}

static void compositor_draw_other(surface_t* other, const rect_t* rect)
{
    rect_t otherRect = SURFACE_RECT(other);
    if (RECT_OVERLAP(rect, &otherRect))
    {
        rect_t overlapRect = *rect;
        RECT_FIT(&overlapRect, &otherRect);
        RECT_FIT(&overlapRect, other->type == SURFACE_WINDOW ? &clientRect : &screenRect);

        screen_transfer(other, &overlapRect);
    }
}

static void compositor_draw_others(compositor_ctx_t* ctx, surface_t* window, const rect_t* rect)
{
    compositor_draw_other(ctx->wall, rect);

    surface_t* other;
    LIST_FOR_EACH(other, ctx->windows, dwmEntry)
    {
        if (other == window)
        {
            continue;
        }

        compositor_draw_other(other, rect);
    }

    LIST_FOR_EACH(other, ctx->panels, dwmEntry)
    {
        if (other == window)
        {
            continue;
        }

        compositor_draw_other(other, rect);
    }
}

static void compositor_invalidate_windows_above(compositor_ctx_t* ctx, surface_t* window, const rect_t* rect)
{
    surface_t* other;
    LIST_FOR_EACH_FROM(other, window->dwmEntry.next, ctx->windows, dwmEntry)
    {
        rect_t otherRect = SURFACE_RECT(other);
        if (RECT_OVERLAP(rect, &otherRect))
        {
            rect_t invalidRect = *rect;
            RECT_FIT(&invalidRect, &otherRect);
            invalidRect.left -= otherRect.left;
            invalidRect.top -= otherRect.top;
            invalidRect.right -= otherRect.left;
            invalidRect.bottom -= otherRect.top;

            other->invalid = true;
            gfx_invalidate(&other->gfx, &invalidRect);
        }
    }
}

static void compositor_draw_cursor(compositor_ctx_t* ctx)
{
    if (ctx->cursor == NULL)
    {
        return;
    }

    rect_t cursorRect = SURFACE_RECT(ctx->cursor);
    RECT_FIT(&cursorRect, &screenRect);
    screen_transfer_blend(ctx->cursor, &cursorRect);
    ctx->cursor->prevRect = cursorRect;
}

void compositor_redraw_cursor(compositor_ctx_t* ctx)
{
    rect_t prevRect = ctx->cursor->prevRect;
    RECT_FIT(&prevRect, &screenRect);
    compositor_draw_others(ctx, NULL, &prevRect);

    compositor_draw_cursor(ctx);

    screen_swap();
}

static void compositor_draw_wall(compositor_ctx_t* ctx)
{
    surface_t* wall = ctx->wall;
    if (!wall->invalid && !wall->moved)
    {
        return;
    }
    wall->invalid = false;
    wall->moved = false;

    rect_t wallRect = SURFACE_RECT(wall);
    RECT_FIT(&wallRect, &clientRect);
    screen_transfer(wall, &wallRect);

    surface_t* window;
    LIST_FOR_EACH(window, ctx->windows, dwmEntry)
    {
        window->moved = true;
    }
}

static void compositor_draw_window_panel(compositor_ctx_t* ctx, surface_t* surface)
{
    rect_t* fitRect = surface->type == SURFACE_WINDOW ? &clientRect : &screenRect;

    rect_t rect;
    if (surface->moved)
    {
        rect = SURFACE_RECT(surface);
        RECT_FIT(&rect, fitRect);

        rect_subtract_t subtract;
        RECT_SUBTRACT(&subtract, &surface->prevRect, &rect);

        for (uint64_t i = 0; i < subtract.count; i++)
        {
            compositor_draw_others(ctx, surface, &surface->prevRect);
        }

        surface->moved = false;
        surface->invalid = false;
        surface->prevRect = rect;
    }
    else if (surface->invalid)
    {
        rect = SURFACE_INVALID_RECT(surface);
        RECT_FIT(&rect, fitRect);

        surface->invalid = false;
    }
    else
    {
        return;
    }

    screen_transfer(surface, &rect);
    if (surface->type == SURFACE_WINDOW)
    {
        compositor_invalidate_windows_above(ctx, surface, &rect);
    }
    surface->gfx.invalidRect = (rect_t){0};
}

static void compositor_draw_windows_panels(compositor_ctx_t* ctx)
{
    surface_t* panel;
    LIST_FOR_EACH(panel, ctx->panels, dwmEntry)
    {
        compositor_draw_window_panel(ctx, panel);
    }

    surface_t* window;
    LIST_FOR_EACH(window, ctx->windows, dwmEntry)
    {
        compositor_draw_window_panel(ctx, window);
    }
}

void compositor_draw(compositor_ctx_t* ctx)
{
    if (!redrawNeeded)
    {
        return;
    }
    redrawNeeded = false;
    if (ctx->wall == NULL)
    {
        return;
    }

    compositor_compute_client_area(ctx);
    compositor_draw_wall(ctx);
    compositor_draw_windows_panels(ctx);
    compositor_draw_cursor(ctx);

    screen_swap();
}

void compositor_set_redraw_needed(void)
{
    redrawNeeded = true;
}

bool compositor_redraw_needed(void)
{
    return redrawNeeded;
}
