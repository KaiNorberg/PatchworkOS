#include "start_menu.h"

#include <stdio.h>
#include <sys/gfx.h>
#include <sys/win.h>
#include <time.h>

#define TOPBAR_HEIGHT 43
#define TOPBAR_PADDING 5

#define START_WIDTH 75
#define START_ID 0

#define TIME_WIDTH 150

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        rect_t rect =
            RECT_INIT_DIM(TOPBAR_PADDING, TOPBAR_PADDING + winTheme.edgeWidth, START_WIDTH, TOPBAR_HEIGHT - TOPBAR_PADDING * 2);
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

        win_timer_set(window, 0);
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
    case LMSG_TIMER:
    {
        gfx_t gfx;
        win_draw_begin(window, &gfx);

        rect_t rect = RECT_INIT_GFX(&gfx);

        rect_t timeRect = RECT_INIT_DIM(RECT_WIDTH(&rect) - TOPBAR_PADDING - TIME_WIDTH, TOPBAR_PADDING + winTheme.edgeWidth,
            TIME_WIDTH, TOPBAR_HEIGHT - TOPBAR_PADDING * 2);
        gfx_edge(&gfx, &timeRect, winTheme.edgeWidth, winTheme.shadow, winTheme.highlight);
        RECT_SHRINK(&timeRect, winTheme.edgeWidth);
        gfx_rect(&gfx, &timeRect, winTheme.background);

        time_t epoch = time(NULL);
        struct tm timeData;
        localtime_r(&epoch, &timeData);
        char buffer[MAX_PATH];
        sprintf(buffer, "%02d:%02d %d-%02d-%02d", timeData.tm_hour, timeData.tm_min, timeData.tm_year + 1900, timeData.tm_mon + 1,
            timeData.tm_mday);
        gfx_text(&gfx, win_font(window), &timeRect, GFX_CENTER, GFX_CENTER, 16, buffer, winTheme.dark, 0);

        win_draw_end(window, &gfx);

        win_timer_set(window, SEC);
    }
    break;
    }

    return 0;
}

int main(void)
{
    rect_t rect;
    win_screen_rect(&rect);
    rect.top = rect.bottom - TOPBAR_HEIGHT;

    win_t* window = win_new("Taskbar", &rect, DWM_PANEL, WIN_NONE, procedure);

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(window, &msg, NEVER);
        win_dispatch(window, &msg);
    }

    win_free(window);
    return 0;
}
