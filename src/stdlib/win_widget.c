#include "_AUX/rect_t.h"
#include "sys/dwm.h"
#include "sys/mouse.h"
#ifndef __EMBED__

#include <stdlib.h>
#include <sys/win.h>

typedef struct
{
    bool pressed;
} button_t;

static void button_draw(widget_t* widget, win_t* window)
{
    button_t* button = win_widget_private(widget);

    surface_t surface;
    win_draw_begin(window, &surface);

    win_theme_t theme;
    win_theme(&theme);
    rect_t rect;
    win_widget_rect(widget, &rect);

    gfx_edge(&surface, &rect, theme.edgeWidth, theme.shadow, theme.shadow);
    RECT_SHRINK(&rect, theme.edgeWidth);

    gfx_rect(&surface, &rect, theme.background);
    if (button->pressed)
    {
        gfx_edge(&surface, &rect, theme.edgeWidth, theme.shadow, theme.highlight);
    }
    else
    {
        gfx_edge(&surface, &rect, theme.edgeWidth, theme.highlight, theme.shadow);
    }

    win_draw_end(window, &surface);
}

uint64_t win_widget_button(widget_t* widget, win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case WMSG_INIT:
    {
        button_t* button = malloc(sizeof(button_t));
        button->pressed = false;

        win_widget_private_set(widget, button);
    }
    break;
    case WMSG_FREE:
    {
        free(win_widget_private(widget));
    }
    break;
    case WMSG_MOUSE:
    {
        msg_mouse_t* data = (msg_mouse_t*)msg->data;
        button_t* button = win_widget_private(widget);

        bool prev = button->pressed;
        button->pressed = data->buttons & MOUSE_LEFT;

        point_t cursorPos = data->pos;
        win_screen_to_client(window, &cursorPos);
        rect_t rect;
        win_widget_rect(widget, &rect);
        if (RECT_CONTAINS_POINT(&rect, cursorPos.x, cursorPos.y))
        {
            if (button->pressed != prev)
            {
                lmsg_button_t msg = {
                    .id = win_widget_id(widget),
                    .pressed = button->pressed,
                };
                win_send(window, LMSG_BUTTON, &msg, sizeof(lmsg_button_t));
            }
        }
        else
        {
            button->pressed = false;
        }

        if (button->pressed != prev)
        {
            button_draw(widget, window);
        }
    }
    break;
    case WMSG_REDRAW:
    {
        button_draw(widget, window);
    }
    break;
    }

    return 0;
}

#endif
