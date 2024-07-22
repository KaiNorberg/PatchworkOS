#ifndef __EMBED__

#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/mouse.h>
#include <sys/win.h>

typedef struct
{
    bool pressed;
    win_text_prop_t props;
} button_t;

static void button_draw(widget_t* widget, win_t* window, win_theme_t* theme, bool redraw)
{
    button_t* button = win_widget_private(widget);

    rect_t rect;
    win_widget_rect(widget, &rect);

    gfx_t gfx;
    win_draw_begin(window, &gfx);

    if (redraw)
    {
        gfx_rim(&gfx, &rect, theme->rimWidth, theme->dark);
    }
    RECT_SHRINK(&rect, theme->rimWidth);

    if (button->pressed)
    {
        gfx_edge(&gfx, &rect, theme->edgeWidth, theme->shadow, theme->highlight);
    }
    else
    {
        gfx_edge(&gfx, &rect, theme->edgeWidth, theme->highlight, theme->shadow);
    }
    RECT_SHRINK(&rect, theme->edgeWidth);

    if (redraw)
    {
        gfx_rect(&gfx, &rect, theme->background);
        gfx_psf(&gfx, win_font(window), &rect, button->props.xAlign, button->props.yAlign, button->props.height,
            win_widget_name(widget), button->props.foreground, button->props.background);
    }

    win_draw_end(window, &gfx);
}

uint64_t win_button_proc(widget_t* widget, win_t* window, const msg_t* msg)
{
    static win_theme_t theme;

    switch (msg->type)
    {
    case WMSG_INIT:
    {
        win_theme(&theme);

        button_t* button = malloc(sizeof(button_t));
        button->pressed = false;
        button->props = WIN_TEXT_PROP_DEFAULT();
        win_widget_private_set(widget, button);
    }
    break;
    case WMSG_FREE:
    {
        free(win_widget_private(widget));
    }
    break;
    case WMSG_TEXT_PROP:
    {
        wmsg_text_prop_t* data = (wmsg_text_prop_t*)msg->data;
        button_t* button = win_widget_private(widget);
        button->props = *data;
    }
    break;
    case WMSG_MOUSE:
    {
        wmsg_mouse_t* data = (wmsg_mouse_t*)msg->data;
        button_t* button = win_widget_private(widget);

        bool prevPressed = button->pressed;

        rect_t rect;
        win_widget_rect(widget, &rect);
        point_t cursorPos = data->pos;
        win_screen_to_client(window, &cursorPos);

        if (RECT_CONTAINS_POINT(&rect, &cursorPos))
        {
            if (data->pressed & MOUSE_LEFT && !button->pressed)
            {
                button->pressed = true;
                lmsg_button_t msg = {.id = win_widget_id(widget), .type = LMSG_BUTTON_PRESS};
                win_send(window, LMSG_BUTTON, &msg, sizeof(lmsg_button_t));
            }
            else if (data->released & MOUSE_LEFT && button->pressed)
            {
                button->pressed = false;
                lmsg_button_t msg = {.id = win_widget_id(widget), .type = LMSG_BUTTON_RELEASED};
                win_send(window, LMSG_BUTTON, &msg, sizeof(lmsg_button_t));
            }
        }
        else
        {
            button->pressed = false;
        }

        if (button->pressed != prevPressed)
        {
            button_draw(widget, window, &theme, false);
        }
    }
    break;
    case WMSG_REDRAW:
    {
        button_draw(widget, window, &theme, true);
    }
    break;
    }

    return 0;
}

#endif
