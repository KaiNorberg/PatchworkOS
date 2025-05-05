#include "platform/platform.h"
#if _PLATFORM_HAS_WIN

#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/mouse.h>
#include <sys/win.h>

typedef struct
{
    bool pressed;
    win_text_prop_t props;
    win_button_flags_t flags;
} button_t;

static void button_draw(widget_t* widget, win_t* window, bool redraw)
{
    button_t* button = win_widget_private(widget);

    rect_t rect;
    win_widget_rect(widget, &rect);

    gfx_t gfx;
    win_draw_begin(window, &gfx);

    if (redraw)
    {
        gfx_rim(&gfx, &rect, windowTheme.rimWidth, windowTheme.dark);
    }
    RECT_SHRINK(&rect, windowTheme.rimWidth);

    if (button->pressed)
    {
        gfx_edge(&gfx, &rect, windowTheme.edgeWidth, windowTheme.shadow, windowTheme.highlight);
    }
    else
    {
        gfx_edge(&gfx, &rect, windowTheme.edgeWidth, windowTheme.highlight, windowTheme.shadow);
    }
    RECT_SHRINK(&rect, windowTheme.edgeWidth);

    if (redraw)
    {
        gfx_rect(&gfx, &rect, button->props.background);
        gfx_text(&gfx, win_font(window), &rect, button->props.xAlign, button->props.yAlign, button->props.height,
            win_widget_name(widget), button->props.foreground, button->props.background);
    }

    win_draw_end(window, &gfx);
}

uint64_t win_button_proc(widget_t* widget, win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case WMSG_INIT:
    {
        button_t* button = malloc(sizeof(button_t));
        button->pressed = false;
        button->props = WIN_TEXT_PROP_DEFAULT();
        button->flags = WIN_BUTTON_NONE;
        win_widget_private_set(widget, button);
    }
    break;
    case WMSG_FREE:
    {
        free(win_widget_private(widget));
    }
    break;
    case WMSG_BUTTON_PROP:
    {
        wmsg_button_prop_t* data = (wmsg_button_prop_t*)msg->data;
        button_t* button = win_widget_private(widget);
        button->props = data->props;
        button->flags = data->flags;
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

        if (button->flags & WIN_BUTTON_TOGGLE)
        {
            if (RECT_CONTAINS_POINT(&rect, &cursorPos) && data->pressed & MOUSE_LEFT)
            {
                button->pressed = !button->pressed;
                lmsg_command_t msg = {.id = win_widget_id(widget),
                    .type = button->pressed ? LMSG_COMMAND_PRESS : LMSG_COMMAND_RELEASE};
                win_send(window, LMSG_COMMAND, &msg, sizeof(lmsg_command_t));
            }
        }
        else
        {
            if (RECT_CONTAINS_POINT(&rect, &cursorPos))
            {
                if (data->pressed & MOUSE_LEFT && !button->pressed)
                {
                    button->pressed = true;
                    lmsg_command_t msg = {.id = win_widget_id(widget), .type = LMSG_COMMAND_PRESS};
                    win_send(window, LMSG_COMMAND, &msg, sizeof(lmsg_command_t));
                }
                else if (data->released & MOUSE_LEFT && button->pressed)
                {
                    button->pressed = false;
                    lmsg_command_t msg = {.id = win_widget_id(widget), .type = LMSG_COMMAND_RELEASE};
                    win_send(window, LMSG_COMMAND, &msg, sizeof(lmsg_command_t));
                }
            }
            else
            {
                button->pressed = false;
            }
        }

        if (button->pressed != prevPressed)
        {
            button_draw(widget, window, false);
        }
    }
    break;
    case WMSG_REDRAW:
    {
        button_draw(widget, window, true);
    }
    break;
    }

    return 0;
}

widget_t* win_button_new(win_t* window, const char* name, const rect_t* rect, widget_id_t id, win_text_prop_t* textProp,
    win_button_flags_t flags)
{
    widget_t* button = win_widget_new(window, win_button_proc, name, rect, id);

    wmsg_button_prop_t props = {.props = textProp != NULL ? *textProp : WIN_TEXT_PROP_DEFAULT(), .flags = flags};
    win_widget_send(button, WMSG_BUTTON_PROP, &props, sizeof(wmsg_button_prop_t));

    return button;
}

#endif
