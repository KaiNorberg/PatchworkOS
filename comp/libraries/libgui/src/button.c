#include <libgfx/gfx.h>
#include <libgui/button.h>
#include <libgui/gui.h>
#include <libgui/widget.h>
#include <stdlib.h>
#include <string.h>

#include "gui_internal.h"

#define GUI_BUTTON_BORDER GFX_PIXEL(0xFF, 0x1B, 0x1B, 0x1B)
#define GUI_BUTTON_NORMAL GFX_PIXEL(0xFF, 0x4B, 0x4B, 0x4B)

typedef struct
{
    gui_widget_t widget;
} gui_button_t;

static status_t gui_button_draw(gui_widget_t* widget, gfx_t* gfx, gfx_rect_t clip)
{
    if (widget == NULL || gfx == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_button_t* button = CONTAINER_OF(widget, gui_button_t, widget);

    gfx_rect_t rect = gui_widget_get_local_bounds(widget);
    gfx_draw_smooth_rect_border(gfx, rect, 4, 1, GUI_BUTTON_NORMAL, GUI_BUTTON_BORDER, GFX_BLEND_ALPHA);

    printf("button draw\n");
    return OK;
}

static gui_class_t buttonClass = {
    .size = sizeof(gui_button_t),
    .draw = gui_button_draw,
};

status_t gui_button_new(gui_widget_t* parent, gui_widget_id_t id, gfx_rect_t bounds, gui_widget_t** out)
{
    if (out == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_widget_t* button;
    status_t status = gui_widget_new(&buttonClass, parent, id, bounds, NULL, &button);
    if (IS_ERR(status))
    {
        return status;
    }

    if (out != NULL)
    {
        *out = button;
    }
    return OK;
}