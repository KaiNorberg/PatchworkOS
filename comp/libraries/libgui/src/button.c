#include <libgui/button.h>
#include <libgui/gui.h>
#include <stdlib.h>
#include <string.h>

#include "gui_internal.h"

typedef struct
{
    gui_widget_t widget;
} gui_button_t; 

static gui_class_t buttonClass = {
    .size = sizeof(gui_button_t),
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

    return ERR(USER, IMPL);
}