#pragma once

#include <libc/list.h>
#include <libgfx/gfx.h>
#include <libgui/gui.h>
#include <stdbool.h>

typedef enum
{
    GUI_FLAG_NONE = 0,
    GUI_FLAG_VISIBLE = (1 << 0),
    GUI_FLAG_RESIZABLE = (1 << 1),
} gui_flags_t;

typedef struct gui_context
{
    gui_widget_t* root;
    gui_widget_t* focused;
    gui_widget_t* hovered;
    gui_cursor_t currentCursor;
} gui_context_t;

typedef struct gui_widget
{
    int32_t x;
    int32_t y;
    gfx_t gfx;
    gui_class_t* cls;
    gui_layout_t* layout;
    gui_flags_t flags;
    gui_cursor_t cursor;
    gui_widget_id_t id;
    list_t children;
    list_entry_t entry;
    gui_widget_t* parent;
    gui_context_t* ctx;
    void* userdata;
} gui_widget_t;