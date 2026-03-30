#include "widget_internal.h"

drawable_t* widget_get_drawable(widget_t* widget)
{
    return &widget->drawable;
}

rect_t widget_get_bounds(widget_t* widget)
{
    return widget->bounds;
}

void widget_set_bounds(widget_t* widget, rect_t bounds)
{
    widget->bounds = bounds;
    widget_invalidate_layout(widget);
}

widget_t* widget_get_parent(widget_t* widget)
{
    return widget->parent;
}

void widget_set_parent(widget_t* widget, widget_t* parent)
{
    if (widget->parent == parent)
    {
        return;
    }

    if (widget->parent != NULL && list_contains(&widget->entry))
    {
        widget_t* oldParent = widget->parent;
        list_remove(&widget->entry);
        widget_invalidate_layout(oldParent);
    }

    widget->parent = parent;

    if (parent != NULL)
    {
        list_push_back(&parent->children, &widget->entry);
        widget_invalidate_layout(parent);
    }
}

widget_id_t widget_get_id(widget_t* widget)
{
    return widget->id;
}

void widget_set_id(widget_t* widget, widget_id_t id)
{
    widget->id = id;
}

const char* widget_get_label(widget_t* widget)
{
    return widget->label;
}

void widget_set_label(widget_t* widget, const char* label)
{
    widget->label = label;
    widget_invalidate_layout(widget);
}

const char* widget_get_icon(widget_t* widget)
{
    return widget->icon;
}

void widget_set_icon(widget_t* widget, const char* icon)
{
    widget->icon = icon;
    widget_invalidate_layout(widget);
}

widget_cursor_t widget_get_cursor(widget_t* widget)
{
    return widget->cursor;
}

void widget_set_cursor(widget_t* widget, widget_cursor_t cursor)
{
    widget->cursor = cursor;
}

void* widget_get_userdata(widget_t* widget)
{
    return widget->userdata;
}

void widget_set_userdata(widget_t* widget, void* userdata)
{
    widget->userdata = userdata;
}
