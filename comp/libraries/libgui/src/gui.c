#include <libc/list.h>
#include <libgfx/pixel.h>
#include <libgfx/rect.h>
#include <libgui/gui.h>
#include <stdlib.h>
#include <string.h>

#include "gui_internal.h"

status_t gui_widget_new(gui_class_t* cls, gui_widget_t* parent, gui_widget_id_t id, gfx_rect_t bounds, void* userdata, gui_widget_t** out)
{
    if (out == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_widget_t* widget = malloc(cls->size);
    if (widget == NULL)
    {
        return ERR(USER, NOMEM);
    }
    widget->x = bounds.left;
    widget->y = bounds.top;
    gfx_pixel_t* buffer = malloc(GFX_RECT_AREA(bounds) * sizeof(gfx_pixel_t));
    if (buffer == NULL)
    {
        free(widget);
        return ERR(USER, NOMEM);
    }
    widget->gfx = GFX(buffer, GFX_RECT_WIDTH(bounds), GFX_RECT_HEIGHT(bounds), GFX_RECT_WIDTH(bounds) * sizeof(gfx_pixel_t));
    widget->cls = cls;
    widget->layout = NULL;
    widget->flags = GUI_FLAG_NONE;
    widget->cursor = GUI_CURSOR_NONE;
    widget->id = id;
    list_init(&widget->children);
    list_entry_init(&widget->entry);
    widget->parent = parent;
    if (parent == NULL)
    {
        widget->ctx = malloc(sizeof(gui_context_t));
        if (widget->ctx == NULL)
        {
            free(buffer);
            free(widget);
            return ERR(USER, NOMEM);
        }
        widget->ctx->root = widget;
        widget->ctx->focused = NULL;
        widget->ctx->hovered = NULL;
        widget->ctx->currentCursor = GUI_CURSOR_DEFAULT;
    }
    else
    {
        widget->ctx = parent->ctx;
        list_push_back(&parent->children, &widget->entry);
    }
    widget->userdata = userdata;

    *out = widget;
    return OK;
}

void gui_widget_free(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return;
    }

    if (widget->ctx->focused == widget)
    {
        widget->ctx->focused = NULL;
    }
    if (widget->ctx->hovered == widget)
    {
        widget->ctx->hovered = NULL;
    }

    gui_widget_t* child;
    gui_widget_t* next;
    LIST_FOR_EACH_SAFE(child, next, &widget->children, entry)
    {
        gui_widget_free(child);
    }

    if (widget->parent != NULL)
    {
        list_remove(&widget->entry);
    }
    else
    {
        free(widget->ctx);
    }

    free(widget->gfx.buffer);
    free(widget);
}

void gui_widget_show(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return;
    }

    widget->flags |= GUI_FLAG_VISIBLE;
}

void gui_widget_hide(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return;
    }

    widget->flags &= ~GUI_FLAG_VISIBLE;
}

void gui_widget_invalidate(gui_widget_t* widget, gfx_rect_t area)
{
    if (widget == NULL)
    {
        return;
    }

}

void gui_widget_invalidate_layout(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return;
    }

}

gfx_t* gui_widget_get_gfx(gui_widget_t* widget)
{
    return &widget->gfx;
}

gfx_rect_t gui_widget_get_bounds(gui_widget_t* widget)
{
    return GFX_RECT(widget->x, widget->y, widget->gfx.width, widget->gfx.height);
}

status_t gui_widget_set_bounds(gui_widget_t* widget, gfx_rect_t bounds)
{
    widget->x = bounds.left;
    widget->y = bounds.top;

    size_t newWidth = GFX_RECT_WIDTH(bounds);
    size_t newHeight = GFX_RECT_HEIGHT(bounds);

    if (newWidth != widget->gfx.width || newHeight != widget->gfx.height)
    {
        if (!(widget->flags & GUI_FLAG_RESIZABLE))
        {
            return ERR(USER, INVAL);
        }

        gfx_pixel_t* newBuffer = realloc(widget->gfx.buffer, newWidth * newHeight * sizeof(gfx_pixel_t));
        if (newBuffer == NULL)
        {
            return ERR(USER, NOMEM);
        }
        widget->gfx = GFX(newBuffer, newWidth, newHeight, newWidth * sizeof(gfx_pixel_t));
    }
    widget->gfx.width = newWidth;
    widget->gfx.height = newHeight;

    gui_widget_invalidate_layout(widget);
    return OK;
}

gui_widget_t* gui_widget_get_parent(gui_widget_t* widget)
{
    return widget->parent;
}

void gui_widget_set_parent(gui_widget_t* widget, gui_widget_t* parent)
{
    if (widget->parent == parent)
    {
        return;
    }

    if (widget->parent != NULL && list_contains(&widget->entry))
    {
        gui_widget_t* oldParent = widget->parent;
        list_remove(&widget->entry);
        gui_widget_invalidate_layout(oldParent);
    }

    widget->parent = parent;

    if (parent != NULL)
    {
        list_push_back(&parent->children, &widget->entry);
        gui_widget_invalidate_layout(parent);
    }
}

gui_widget_id_t gui_widget_get_id(gui_widget_t* widget)
{
    return widget->id;
}

void gui_widget_set_id(gui_widget_t* widget, gui_widget_id_t id)
{
    widget->id = id;
}

gui_cursor_t gui_widget_get_cursor(gui_widget_t* widget)
{
    return widget->cursor;
}

void gui_widget_set_cursor(gui_widget_t* widget, gui_cursor_t cursor)
{
    widget->cursor = cursor;
}

void* gui_widget_get_userdata(gui_widget_t* widget)
{
    return widget->userdata;
}

void gui_widget_set_userdata(gui_widget_t* widget, void* userdata)
{
    widget->userdata = userdata;
}
