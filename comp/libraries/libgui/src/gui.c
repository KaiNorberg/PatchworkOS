#include <_libc/clock_t.h>
#include <libc/list.h>
#include <libgfx/pixel.h>
#include <libgfx/rect.h>
#include <libgui/gui.h>
#include <libgui/mouse.h>
#include <stdlib.h>
#include <string.h>

#include "gui_internal.h"

static status_t gui_root_draw(gui_widget_t* widget, gfx_t* gfx)
{
    if (widget == NULL || gfx == NULL)
    {
        return ERR(USER, INVAL);
    }

    gfx_draw_fill(gfx, widget->gui->background);

    if (widget->gui->wallpaper == NULL)
    {
        return OK;
    }

    gfx_rect_t rect = gui_widget_get_local_bounds(widget);
    gui_image_t* img = widget->gui->wallpaper;

    if (widget->gui->wallpaperScale == GUI_IMAGE_SCALE_STRETCH)
    {
        gfx_draw_scale_blit(gfx, &img->gfx, rect, img->gfx.clip, GFX_BLEND_ALPHA);
    }
    else if (widget->gui->wallpaperScale == GUI_IMAGE_SCALE_CENTER)
    {
        gfx_rect_t centered = GFX_RECT_FROM_CENTER(
            rect.left + GFX_RECT_WIDTH(rect) / 2,
            rect.top + GFX_RECT_HEIGHT(rect) / 2,
            img->width, img->height
        );
        gfx_draw_blit(gfx, &img->gfx, centered, img->gfx.clip);
    }
    else if (widget->gui->wallpaperScale == GUI_IMAGE_SCALE_FIT)
    {
        float scaleX = (float)GFX_RECT_WIDTH(rect) / img->width;
        float scaleY = (float)GFX_RECT_HEIGHT(rect) / img->height;
        float scale = MIN(scaleX, scaleY);

        int32_t newW = (int32_t)(img->width * scale);
        int32_t newH = (int32_t)(img->height * scale);

        gfx_rect_t fitRect = GFX_RECT_FROM_CENTER(
            rect.left + GFX_RECT_WIDTH(rect) / 2,
            rect.top + GFX_RECT_HEIGHT(rect) / 2,
            newW, newH
        );
        gfx_draw_scale_blit(gfx, &img->gfx, fitRect, img->gfx.clip, GFX_BLEND_ALPHA);
    }
    else if (widget->gui->wallpaperScale == GUI_IMAGE_SCALE_TILE)
    {
        for (int32_t ty = rect.top; ty < rect.bottom; ty += img->height)
        {
            for (int32_t tx = rect.left; tx < rect.right; tx += img->width)
            {
                gfx_rect_t tileRect = GFX_RECT(tx, ty, img->width, img->height);
                gfx_draw_blit(gfx, &img->gfx, tileRect, img->gfx.clip);
            }
        }
    }
    else
    {
        return ERR(USER, INVAL);
    }

    return OK;
}

static gui_class_t rootClass = {
    .size = sizeof(gui_widget_t),
    .draw = gui_root_draw,
};

status_t gui_new(gfx_t* screen, gui_t** out)
{
    if (screen == NULL || out == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_t* gui = malloc(sizeof(gui_t));
    if (gui == NULL)
    {
        return ERR(USER, NOMEM);
    }

    gui->screen = screen;
    gui->dirty = GFX_REGION();
    gui->background = GFX_PIXEL(0, 0, 0, 0);
    gui->root.bounds = screen->clip;
    gui->root.cls = &rootClass;
    gui->root.layout = NULL;
    gui->root.flags = GUI_FLAG_VISIBLE;
    gui->root.cursor = GUI_CURSOR_NONE;
    gui->root.id = 0;
    list_init(&gui->root.children);
    list_entry_init(&gui->root.entry);
    gui->root.parent = NULL;
    gui->root.gui = gui;
    gui->root.userdata = NULL;
    gui->focused = NULL;
    gui->hovered = NULL;
    gui->mouseButtons = 0;
    gui->wallpaper = NULL;
    gui->wallpaperScale = GUI_IMAGE_SCALE_STRETCH;

    gui_invalidate(gui, gui->root.bounds);

    *out = gui;
    return OK;
}

void gui_free(gui_t* gui)
{
    if (gui == NULL)
    {
        return;
    }

    gui_widget_t* child;
    gui_widget_t* next;
    LIST_FOR_EACH_SAFE(child, next, &gui->root.children, entry)
    {
        gui_widget_free(child);
    }

    free(gui);
}

gui_widget_t* gui_get_root(gui_t* gui)
{
    if (gui == NULL)
    {
        return NULL;
    }

    return &gui->root;
}

gui_widget_t* gui_get_hovered(gui_t* gui)
{
    if (gui == NULL)
    {
        return NULL;
    }

    return gui->hovered;
}

gui_widget_t* gui_get_focused(gui_t* gui)
{
    if (gui == NULL)
    {
        return NULL;
    }

    return gui->focused;
}

void gui_get_dirty(gui_t* gui, gfx_region_t* out)
{
    if (gui == NULL || out == NULL)
    {
        return;
    }

    *out = gui->dirty;
}

void gui_set_background(gui_t* gui, gfx_pixel_t color)
{
    if (gui == NULL)
    {
        return;
    }

    gui->background = color;
    gui_invalidate(gui, gui->root.bounds);
}

void gui_set_wallpaper(gui_t* gui, gui_image_t* image, gui_image_scale_t scale)
{
    if (gui == NULL)
    {
        return;
    }
    gui->wallpaper = image;
    gui->wallpaperScale = scale;
    gui_invalidate(gui, gui->root.bounds);
}

void gui_invalidate(gui_t* gui, gfx_rect_t area)
{
    if (gui == NULL)
    {
        return;
    }

    gfx_region_add(&gui->dirty, area);
}

clock_t gui_next_timeout(gui_t* gui, clock_t now)
{
    UNUSED(gui);
    UNUSED(now);

    /// @todo gui_next_timeout()
    return CLOCKS_NEVER;
}

static status_t gui_draw_widget(gui_t* gui, gui_widget_t* widget, gfx_rect_t clip)
{
    if (gui == NULL || widget == NULL)
    {
        return ERR(USER, INVAL);
    }

    if (!(widget->flags & GUI_FLAG_VISIBLE))
    {
        return OK;
    }

    gfx_rect_t widgetBounds = gui_widget_get_ancestor_bounds(widget);
    if (!GFX_RECT_OVERLAP(widgetBounds, clip))
    {
        return OK;
    }

    gfx_rect_t drawArea = GFX_RECT_INTERSECTION(widgetBounds, clip);
    if (widget->cls != NULL && widget->cls->draw != NULL)
    {
        gfx_t gfx = GFX((void*)((uintptr_t)gui->screen->buffer + (widgetBounds.top * gui->screen->pitch) +
                            (widgetBounds.left * sizeof(gfx_pixel_t))),
            GFX_RECT_WIDTH(widgetBounds), GFX_RECT_HEIGHT(widgetBounds), gui->screen->pitch);

        gfx.clip = GFX_RECT(
            drawArea.left - widgetBounds.left,
            drawArea.top - widgetBounds.top,
            GFX_RECT_WIDTH(drawArea),
            GFX_RECT_HEIGHT(drawArea)
        );

        status_t status = widget->cls->draw(widget, &gfx);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    gui_widget_t* child;
    LIST_FOR_EACH(child, &widget->children, entry)
    {
        status_t status = gui_draw_widget(gui, child, drawArea);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    return OK;
}

status_t gui_tick(gui_t* gui, clock_t delta)
{
    if (gui == NULL)
    {
        return ERR(USER, INVAL);
    }

    /// @todo timers

    for (size_t i = 0; i < gui->dirty.count; i++)
    {
        status_t status = gui_draw_widget(gui, &gui->root, gui->dirty.rects[i]);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    gui->dirty.count = 0;
    return OK;
}

status_t gui_input_mouse(gui_t* gui, int32_t x, int32_t y, int32_t z, gui_mouse_buttons_t pressed, gui_mouse_buttons_t released)
{
    if (gui == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_mouse_buttons_t held = (gui->mouseButtons | pressed) & ~released;
    gui->mouseButtons = held;

    gui_widget_t* hovered = NULL;
    gui_widget_t* current = &gui->root;

    while (current != NULL)
    {
        hovered = current;
        gui_widget_t* child;
        gui_widget_t* next = NULL;
        LIST_FOR_EACH(child, &current->children, entry)
        {
            if (!(child->flags & GUI_FLAG_VISIBLE))
            {
                continue;
            }

            gfx_rect_t bounds = gui_widget_get_ancestor_bounds(child);
            if (GFX_RECT_CONTAINS(bounds, x, y))
            {
                next = child;
                break;
            }
        }
        current = next;
    }

    if (gui->hovered != hovered)
    {
        if (gui->hovered != NULL)
        {
            gui_event_t leave = {
                .type = GUI_EVENT_TYPE_MOUSE_LEAVE,
            };
            status_t status = gui_widget_emit_event(gui->hovered, &leave);
            if (IS_ERR(status))
            {
                return status;
            }
        }

        if (hovered != NULL)
        {
            gui_event_t enter = {
                .type = GUI_EVENT_TYPE_MOUSE_ENTER,
            };
            status_t status = gui_widget_emit_event(hovered, &enter);
            if (IS_ERR(status))
            {
                return status;
            }
        }
        
        gui->hovered = hovered;
    }

    if (pressed & GUI_MOUSE_BUTTON_LEFT)
    {
        if (gui->focused != NULL)
        {
            gui_event_t focusOut = {
                .type = GUI_EVENT_TYPE_FOCUS_OUT,
            };
            status_t status = gui_widget_emit_event(gui->focused, &focusOut);
            if (IS_ERR(status))
            {
                return status;
            }
        }

        if (hovered != NULL)
        {
            gui_event_t focusIn = {
                .type = GUI_EVENT_TYPE_FOCUS_IN,
            };
            status_t status = gui_widget_emit_event(hovered, &focusIn);
            if (IS_ERR(status))
            {
                return status;
            }
        }

        gui->focused = hovered;
    }

    if (hovered != NULL)
    {
        gui_event_t move = {
            .type = GUI_EVENT_TYPE_MOUSE,
            .mouse = {
                .x = x,
                .y = y,
                .z = z,
                .pressed = pressed,
                .released = released,
                .held = held,
            },
        };
        status_t status = gui_widget_emit_event(hovered, &move);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    return OK;
}