#ifndef __EMBED__

#include <stdlib.h>
#include <sys/win.h>

typedef struct
{
    bool pressed;
} button_t;

uint64_t win_widget_button(widget_t* widget, void* private, win_t* window, surface_t* surface, msg_t* msg)
{
    switch (msg->type)
    {
    case WMSG_INIT:
    {
        wmsg_init_t* data = (wmsg_init_t*)msg->data;
        button_t* button = malloc(sizeof(button_t));
        button->pressed = false;
        data->private = button;
    }
    break;
    case WMSG_REDRAW:
    {
        button_t* button = private;

        win_theme_t theme;
        win_theme(&theme);
        rect_t rect;
        win_widget_rect(widget, &rect);

        gfx_rect(surface, &rect, theme.background);
        gfx_edge(surface, &rect, theme.edgeWidth, theme.highlight, theme.shadow);
    }
    break;
    }

    return 0;
}

#endif
