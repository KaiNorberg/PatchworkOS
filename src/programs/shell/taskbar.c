#include "taskbar.h"
#include "start_menu.h"
#include "sys/gfx.h"

#include <sys/win.h>

#define START_PADDING 5
#define START_WIDTH 75
#define START_ID 0

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        rect_t rect =
            RECT_INIT_DIM(START_PADDING, START_PADDING + winTheme.edgeWidth, START_WIDTH, TOPBAR_HEIGHT - START_PADDING * 2);
        widget_t* button = win_button_new(window, "Start", &rect, START_ID, NULL, WIN_BUTTON_TOGGLE);
    }
    break;
    case LMSG_REDRAW:
    {
        gfx_t gfx;
        win_draw_begin(window, &gfx);

        rect_t rect = RECT_INIT_GFX(&gfx);

        gfx_rect(&gfx, &rect, winTheme.background);
        rect.bottom = rect.top + winTheme.edgeWidth;
        gfx_rect(&gfx, &rect, winTheme.bright);

        win_draw_end(window, &gfx);
    }
    break;
    case LMSG_COMMAND:
    {
        lmsg_command_t* data = (lmsg_command_t*)msg->data;
        if (data->id == START_ID)
        {
            if (data->type == LMSG_COMMAND_PRESS)
            {
                start_menu_open();
            }
            else if (data->type == LMSG_COMMAND_RELEASE)
            {
                start_menu_close();
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
