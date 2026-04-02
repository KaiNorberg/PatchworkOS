#include <libgfx/gfx.h>
#include <libgui/button.h>
#include <libgui/gui.h>
#include <libgui/theme.h>
#include <libgui/widget.h>
#include <stdlib.h>
#include <string.h>

#include "gui_internal.h"

#define GUI_BUTTON_BORDER GFX_PIXEL(0xFF, 0x0D, 0x0E, 0x11)
#define GUI_BUTTON_NORMAL GUI_THEME_BACK_1
#define GUI_BUTTON_HOVERED GUI_THEME_BACK_2
#define GUI_BUTTON_PRESSED GUI_THEME_BACK_3

typedef struct
{
    gui_widget_t widget;
    bool pressed;
} gui_button_t;

static status_t gui_button_init(gui_widget_t* widget)
{
    if (widget == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_button_t* button = CONTAINER_OF(widget, gui_button_t, widget);
    button->pressed = false;

    gui_widget_set_cursor(widget, GUI_CURSOR_POINTER);

    return OK;
}

static status_t gui_button_draw(gui_widget_t* widget, gfx_t* gfx, gfx_rect_t clip)
{
    if (widget == NULL || gfx == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_button_t* button = CONTAINER_OF(widget, gui_button_t, widget);

    gfx_pixel_t foreground = GUI_BUTTON_NORMAL;
    if (gui_widget_is_hovered(widget))
    {
        foreground = GUI_BUTTON_HOVERED;
    }
    if (button->pressed)
    {
        foreground = GUI_BUTTON_PRESSED;
    }

    gfx_rect_t rect = gui_widget_get_local_bounds(widget);
    gfx_draw_smooth_rect_border(gfx, rect, GUI_THEME_LARGE_RADIUS, GUI_THEME_BORDER_WIDTH, foreground, GUI_BUTTON_BORDER, GFX_BLEND_ALPHA);

    return OK;
}

static status_t gui_button_procedure(gui_widget_t* widget, gui_event_t* event)
{
    if (widget == NULL || event == NULL)
    {
        return ERR(USER, INVAL);
    }

    gui_button_t* button = CONTAINER_OF(widget, gui_button_t, widget);

    switch (event->type)
    {
    case GUI_EVENT_TYPE_MOUSE:            
        if (event->mouse.pressed & GUI_MOUSE_BUTTON_LEFT)
        {
            button->pressed = true;
            gui_widget_invalidate(widget, gui_widget_get_local_bounds(widget));
        }
        else if (event->mouse.released & GUI_MOUSE_BUTTON_LEFT)
        {
            if (!button->pressed)
            {
                break;
            }

            button->pressed = false;
            gui_widget_invalidate(widget, gui_widget_get_local_bounds(widget));
            if (gui_widget_is_hovered(widget))
            {
                gui_event_t command = {
                    .type = GUI_EVENT_TYPE_COMMAND,
                    .command = {
                        .source = gui_widget_get_id(widget),
                        .command = GUI_EVENT_CMD_CLICK,
                    },
                };
                gui_widget_emit_event(gui_widget_get_parent(widget), &command);
            }
        }
        break;
    case GUI_EVENT_TYPE_MOUSE_ENTER:
        gui_widget_invalidate(widget, gui_widget_get_local_bounds(widget));
        break;
    case GUI_EVENT_TYPE_MOUSE_LEAVE:
        button->pressed = false;
        gui_widget_invalidate(widget, gui_widget_get_local_bounds(widget));
        break;
    case GUI_EVENT_TYPE_FOCUS_IN:
    case GUI_EVENT_TYPE_FOCUS_OUT:
        gui_widget_invalidate(widget, gui_widget_get_local_bounds(widget));
        break;
    default:
        break;
    }

    return OK;
}

static gui_class_t buttonClass = {
    .size = sizeof(gui_button_t),
    .draw = gui_button_draw,
    .init = gui_button_init,
    .procedure = gui_button_procedure,
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