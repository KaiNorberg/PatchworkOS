#pragma once

#include <libc/list.h>
#include <libdraw/draw.h>
#include <libdraw/pixel.h>
#include <libdraw/polygon.h>
#include <libdraw/rect.h>
#include <libdraw/vertices.h>
#include <libwidget/widget.h>
#include <stdbool.h>

typedef enum
{
    WIDGET_FLAG_NONE = 0,
    WIDGET_FLAG_VISIBLE = (1 << 0),
    WIDGET_FLAG_ENABLED = (1 << 1),
    WIDGET_FLAG_FOCUSABLE = (1 << 2),
    WIDGET_FLAG_LAYOUT_DIRTY = (1 << 3),
    WIDGET_FLAG_TRANSPARENT = (1 << 4),
} widget_flags_t;

typedef struct widget_context
{
    widget_t* root;
    widget_t* focused;
    widget_t* hovered;
    widget_cursor_t currentCursor;
} widget_context_t;

typedef struct widget
{
    widget_class_t* cls;
    widget_layout_t* layout;
    rect_t bounds;
    size_t desiredWidth;
    size_t desiredHeight;
    drawable_t drawable;
    widget_flags_t flags;
    widget_cursor_t cursor;
    widget_id_t id;
    const char* label;
    const char* icon;
    void* userdata;
    widget_context_t* ctx;
    struct widget* parent;
    list_t children;
    list_entry_t entry;
} widget_t;