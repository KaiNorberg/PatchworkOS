#include <libc/list.h>
#include <libgfx/pixel.h>
#include <libgfx/rect.h>
#include <libgui/gui.h>
#include <stdlib.h>
#include <string.h>

#include "gui_internal.h"

typedef enum gui_flags
{
    GUI_FLAG_NONE = 0,
    GUI_FLAG_VISIBLE = (1 << 0),
    GUI_FLAG_RESIZABLE = (1 << 1),
} gui_flags_t;

typedef struct gui_widget
{
    gui_flags_t flags;
    gui_cursor_t cursor;
    list_t children;
    list_entry_t entry;
    gui_widget_t* parent;
} gui_widget_t;

status_t gui_widget_new(gui_widget_t* parent, gui_widget_t** out)
{
    if (parent == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_widget_t* widget = malloc(sizeof(gui_widget_t));
    if (widget == NULL)
    {
        return ERR(USER, NOMEM);
    }
    widget->flags = GUI_FLAG_NONE;
    widget->cursor = GUI_CURSOR_NONE;
    list_init(&widget->children);
    list_entry_init(&widget->entry);
    widget->parent = parent;
    list_push_back(&parent->children, &widget->entry);

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

    /*if (widget->gui->focused == widget)
    {
        widget->gui->focused = NULL;
    }
    if (widget->gui->hovered == widget)
    {
        widget->gui->hovered = NULL;
    }*/

    gui_widget_t* child;
    gui_widget_t* next;
    LIST_FOR_EACH_SAFE(child, next, &widget->children, entry)
    {
        gui_widget_free(child);
    }

    list_remove(&widget->entry);

    free(widget);
}

/*void gui_widget_show(gui_widget_t* widget)
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
}*/

void gui_widget_invalidate(gui_widget_t* widget, gfx_rect_t area)
{
    if (widget == NULL)
    {
        return;
    }

    /*gfx_rect_t widgetBounds = gui_widget_get_ancestor_bounds(widget);
    gfx_rect_t absoluteArea = GFX_RECT_OFFSET(area, widgetBounds.left, widgetBounds.top);
    gfx_rect_t invalidArea = GFX_RECT_INTERSECTION(widgetBounds, absoluteArea);

    gui_invalidate(widget->gui, invalidArea);*/
}

gui_widget_t* gui_widget_get_parent(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return NULL;
    }

    return widget->parent;
}
