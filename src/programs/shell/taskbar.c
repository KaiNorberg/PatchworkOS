#include "taskbar.h"

#include <sys/win.h>

#define TOPBAR_HEIGHT 45

#define START_PADDING 5
#define START_WIDTH 100
#define START_ID 0

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    static win_theme_t theme;

    switch (msg->type)
    {
    case LMSG_INIT:
    {
        win_theme(&theme);

        //rect_t rect = RECT_INIT_DIM(START_PADDING * 2, START_PADDING + theme.edgeWidth, START_WIDTH, TOPBAR_HEIGHT - START_PADDING * 2);
        //widget_t* button = win_widget_new(window, win_button_proc, "Start", &rect, START_ID);
    }
    break;
    case LMSG_REDRAW:
    {
        gfx_t gfx;
        win_draw_begin(window, &gfx);

        rect_t rect;
        win_client_rect(window, &rect);

        gfx_rect(&gfx, &rect, theme.background);
        rect.bottom = rect.top + theme.edgeWidth;
        gfx_rect(&gfx, &rect, theme.bright);

        win_draw_end(window, &gfx);
    }
    break;
    case LMSG_BUTTON:
    {
        lmsg_button_t* data = (lmsg_button_t*)msg->data;
        if (data->type == LMSG_BUTTON_RELEASED)
        {
            if (data->id == START_ID)
            {

            }
        }
    }
    break;
    }

    return 0;
}

win_t* taskbar_new(void)
{
    rect_t rect;
    win_screen_rect(&rect);
    rect.top = rect.bottom - TOPBAR_HEIGHT;

    return win_new("Taskbar", &rect, DWM_PANEL, WIN_NONE, procedure);
}
