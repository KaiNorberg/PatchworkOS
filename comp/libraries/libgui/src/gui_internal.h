#pragma once

#include <libc/list.h>
#include <libgfx/gfx.h>
#include <libgui/event.h>
#include <libgui/gui.h>
#include <stdbool.h>

typedef enum gui_flags
{
    GUI_FLAG_NONE = 0,
    GUI_FLAG_VISIBLE = (1 << 0),
    GUI_FLAG_RESIZABLE = (1 << 1),
} gui_flags_t;

typedef struct gui_widget
{
    gfx_rect_t bounds;
    gui_class_t* cls;
    gui_layout_t* layout;
    gui_flags_t flags;
    gui_cursor_t cursor;
    gui_widget_id_t id;
    list_t children;
    list_entry_t entry;
    gui_widget_t* parent;
    gui_t* gui;
    void* userdata;
} gui_widget_t;

typedef struct gui
{
    gfx_t* screen;
    gfx_region_t dirty;
    gui_widget_t root;
    gui_widget_t* focused;
    gui_widget_t* hovered;
    gui_mouse_buttons_t mouseButtons;
} gui_t;