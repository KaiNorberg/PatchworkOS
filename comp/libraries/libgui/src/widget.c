#include <libc/list.h>
#include <libgfx/pixel.h>
#include <libgfx/rect.h>
#include <libgui/gui.h>
#include <stdlib.h>
#include <string.h>

#include "gui_internal.h"

status_t gui_widget_new(gui_class_t* cls, gui_widget_t* parent, gui_widget_id_t id, gfx_rect_t bounds, void* userdata,
    gui_widget_t** out)
{
    if (cls == NULL || parent == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_widget_t* widget = malloc(cls->size);
    if (widget == NULL)
    {
        return ERR(USER, NOMEM);
    }
    widget->bounds = bounds;
    widget->cls = cls;
    widget->layout = NULL;
    widget->flags = GUI_FLAG_NONE;
    widget->cursor = GUI_CURSOR_NONE;
    widget->id = id;
    list_init(&widget->children);
    list_entry_init(&widget->entry);
    widget->parent = parent;
    list_push_back(&parent->children, &widget->entry);
    widget->gui = widget->parent->gui;
    widget->userdata = userdata;

    if (cls->init != NULL)
    {
        status_t status = cls->init(widget);
        if (IS_ERR(status))
        {
            gui_widget_free(widget);
            return status;
        }
    }

    if (out != NULL)
    {
        *out = widget;
    }

    return OK;
}

void gui_widget_free(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return;
    }

    if (widget->cls->deinit != NULL)
    {
        widget->cls->deinit(widget);
    }

    if (widget->gui->focused == widget)
    {
        widget->gui->focused = NULL;
    }
    if (widget->gui->hovered == widget)
    {
        widget->gui->hovered = NULL;
    }

    gui_widget_t* child;
    gui_widget_t* next;
    LIST_FOR_EACH_SAFE(child, next, &widget->children, entry)
    {
        gui_widget_free(child);
    }

    list_remove(&widget->entry);

    free(widget);
}

void gui_widget_show(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return;
    }

    widget->flags |= GUI_FLAG_VISIBLE;
    gui_widget_invalidate(widget, gui_widget_get_local_bounds(widget));

    gui_widget_t* child;
    gui_widget_t* temp;
    LIST_FOR_EACH_SAFE(child, temp, &widget->children, entry)
    {
        gui_widget_show(child);
    }
}

void gui_widget_hide(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return;
    }

    gui_widget_invalidate(widget, gui_widget_get_local_bounds(widget));
    widget->flags &= ~GUI_FLAG_VISIBLE;

    gui_widget_t* child;
    gui_widget_t* temp;
    LIST_FOR_EACH_SAFE(child, temp, &widget->children, entry)
    {
        gui_widget_hide(child);
    }
}

void gui_widget_invalidate(gui_widget_t* widget, gfx_rect_t area)
{
    if (widget == NULL)
    {
        return;
    }

    gfx_rect_t widgetBounds = gui_widget_get_ancestor_bounds(widget);
    gfx_rect_t absoluteArea = GFX_RECT_OFFSET(area, widgetBounds.left, widgetBounds.top);
    gfx_rect_t invalidArea = GFX_RECT_INTERSECTION(widgetBounds, absoluteArea);

    gui_invalidate(widget->gui, invalidArea);        
}

void gui_widget_invalidate_layout(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return;
    }
}

gfx_rect_t gui_widget_get_bounds(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return GFX_RECT(0, 0, 0, 0);
    }

    return widget->bounds;
}

gfx_rect_t gui_widget_get_ancestor_bounds(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return GFX_RECT(0, 0, 0, 0);
    }

    gfx_rect_t bounds = widget->bounds;
    gui_widget_t* parent = widget->parent;
    while (parent != NULL)
    {
        bounds.left += parent->bounds.left;
        bounds.top += parent->bounds.top;
        bounds.right += parent->bounds.left;
        bounds.bottom += parent->bounds.top;
        parent = parent->parent;
    }

    return bounds;
}

gfx_rect_t gui_widget_get_local_bounds(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return GFX_RECT(0, 0, 0, 0);
    }

    return GFX_RECT(0, 0, GFX_RECT_WIDTH(widget->bounds), GFX_RECT_HEIGHT(widget->bounds));
}

status_t gui_widget_set_bounds(gui_widget_t* widget, gfx_rect_t bounds)
{
    if (widget == NULL)
    {
        return ERR(USER, INVAL);
    }

    widget->bounds = bounds;
    gui_widget_invalidate_layout(widget->parent);
    return OK;
}

gui_widget_t* gui_widget_get_parent(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return NULL;
    }

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
    if (widget == NULL)
    {
        return 0;
    }

    return widget->id;
}

void gui_widget_set_id(gui_widget_t* widget, gui_widget_id_t id)
{
    if (widget == NULL)
    {
        return;
    }

    widget->id = id;
}

gui_cursor_t gui_widget_get_cursor(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return GUI_CURSOR_NONE;
    }

    gui_cursor_t cursor = GUI_CURSOR_NONE;
    while (widget != NULL && cursor == GUI_CURSOR_NONE)
    {
        cursor = widget->cursor;
        widget = widget->parent;
    }

    return cursor;
}

void gui_widget_set_cursor(gui_widget_t* widget, gui_cursor_t cursor)
{
    if (widget == NULL)
    {
        return;
    }

    widget->cursor = cursor;
}

void* gui_widget_get_userdata(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return NULL;
    }

    return widget->userdata;
}

void gui_widget_set_userdata(gui_widget_t* widget, void* userdata)
{
    if (widget == NULL)
    {
        return;
    }

    widget->userdata = userdata;
}

bool gui_widget_is_visible(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return false;
    }

    return (widget->flags & GUI_FLAG_VISIBLE) != 0;
}

bool gui_widget_is_hovered(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return false;
    }

    return widget->gui->hovered == widget;
}

bool gui_widget_is_focused(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return false;
    }

    return widget->gui->focused == widget;
}

status_t gui_widget_emit_event(gui_widget_t* widget, gui_event_t* event)
{
    if (widget == NULL || event == NULL)
    {
        return ERR(USER, INVAL);
    }

    if (widget->cls != NULL && widget->cls->procedure != NULL)
    {
        return widget->cls->procedure(widget, event);
    }

    return OK;
}