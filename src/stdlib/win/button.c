#ifndef __EMBED__

#include <stdlib.h>
#include <string.h>
#include <sys/gfx.h>
#include <sys/mouse.h>
#include <sys/win.h>

typedef struct
{
    bool pressed;
    char* label;
    uint64_t height;
    pixel_t foreground;
    pixel_t background;
    gfx_align_t xAlign;
    gfx_align_t yAlign;
} button_t;

static void button_draw(widget_t* widget, win_t* window, bool redraw)
{
    button_t* button = win_widget_private(widget);

    gfx_t gfx;
    win_draw_begin(window, &gfx);
    win_theme_t theme;
    win_theme(&theme);
    rect_t rect;
    win_widget_rect(widget, &rect);

    if (redraw)
    {
        gfx_rim(&gfx, &rect, theme.rimWidth, theme.dark);
    }
    RECT_SHRINK(&rect, theme.rimWidth);

    if (redraw)
    {
        gfx_rect(&gfx, &rect, theme.background);
        gfx_psf(&gfx, win_font(window), &rect, button->xAlign, button->yAlign, button->height, button->label, button->foreground,
            button->background);
    }

    if (button->pressed)
    {
        gfx_edge(&gfx, &rect, theme.edgeWidth, theme.shadow, theme.highlight);
    }
    else
    {
        gfx_edge(&gfx, &rect, theme.edgeWidth, theme.highlight, theme.shadow);
    }

    win_draw_end(window, &gfx);
}

uint64_t win_widget_button(widget_t* widget, win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case WMSG_INIT:
    {
        button_t* button = malloc(sizeof(button_t));
        button->pressed = false;
        button->height = 16;
        button->background = 0;
        button->foreground = 0xFF000000;
        button->xAlign = GFX_CENTER;
        button->yAlign = GFX_CENTER;

        const char* label = win_widget_name(widget);
        button->label = malloc(strlen(label) + 1);
        strcpy(button->label, label);

        win_widget_private_set(widget, button);
    }
    break;
    case WMSG_FREE:
    {
        button_t* button = win_widget_private(widget);

        free(button->label);
        free(win_widget_private(widget));
    }
    break;
    case WMSG_SET_TEXT:
    {
        wmsg_set_text* data = (wmsg_set_text*)msg->data;
        button_t* button = win_widget_private(widget);

        if (data->text != NULL)
        {
            button->label = realloc(button->label, strlen(data->text) + 1);
            strcpy(button->label, data->text);
        }

        button->height = data->height;
        button->foreground = data->foreground;
        button->background = data->background;
        button->xAlign = data->xAlign;
        button->yAlign = data->yAlign;
    }
    break;
    case WMSG_MOUSE:
    {
        wmsg_mouse_t* data = (wmsg_mouse_t*)msg->data;
        button_t* button = win_widget_private(widget);

        bool prevPressed = button->pressed;
        button->pressed = data->buttons & MOUSE_LEFT;

        point_t cursorPos = data->pos;
        win_screen_to_client(window, &cursorPos);
        rect_t rect;
        win_widget_rect(widget, &rect);

        if (RECT_CONTAINS_POINT(&rect, cursorPos.x, cursorPos.y))
        {
            if (button->pressed != prevPressed)
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

#endif
