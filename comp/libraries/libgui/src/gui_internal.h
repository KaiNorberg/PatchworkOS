#pragma once

#include <libc/list.h>
#include <libgfx/gfx.h>
#include <libgfx/pixel.h>
#include <libgui/gui.h>
#include <stdbool.h>

typedef struct gui
{
    gfx_t* screen;
    gfx_pixel_t background;
    gfx_region_t dirty;
    gui_widget_t* focused;
    gui_widget_t* hovered;
    gui_mouse_buttons_t mouseButtons;
} gui_t;