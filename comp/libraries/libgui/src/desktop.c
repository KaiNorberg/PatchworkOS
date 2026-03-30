#include <libgfx/pixel.h>
#include <libgui/desktop.h>
#include <libgui/gui.h>
#include <stdlib.h>
#include <string.h>

#include "gui_internal.h"

typedef struct
{
    gui_widget_t widget;
    gfx_pixel_t* screen;
} gui_desktop_t; 

static status_t gui_desktop_init(gui_widget_t* widget);

static gui_class_t desktopClass = {
    .size = sizeof(gui_desktop_t),
    .init = gui_desktop_init,
};

static status_t gui_desktop_init(gui_widget_t* widget)
{
    gui_desktop_t* desktop = CONTAINER_OF(widget, gui_desktop_t, widget);
    desktop->screen = widget->userdata;
    widget->userdata = NULL;

    return OK;
}

status_t gui_desktop_new(gfx_rect_t bounds, gfx_pixel_t* screen, gui_widget_t** out)
{
    if (out == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_widget_t* desktop;
    status_t status = gui_widget_new(&desktopClass, NULL, UINT32_MAX, bounds, screen, &desktop);
    if (IS_ERR(status))
    {
        return status;
    }

    return ERR(USER, IMPL);
}