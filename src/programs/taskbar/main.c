#include "start_menu.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <libdwm/dwm.h>

#define TOPBAR_HEIGHT 43
#define TOPBAR_PADDING 5

#define START_WIDTH 75
#define START_ID 0

#define TIME_WIDTH 150

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case EVENT_INIT:
    {
        /*rect_t rect =
            RECT_INIT_DIM(TOPBAR_PADDING, TOPBAR_PADDING + winTheme.edgeWidth, START_WIDTH, TOPBAR_HEIGHT - TOPBAR_PADDING * 2);

        win_text_prop_t textProp = WIN_TEXT_PROP_DEFAULT();
        textProp.background = winTheme.background;

        widget_t* button = win_button_new(window, "Start", &rect, START_ID, &textProp, WIN_BUTTON_TOGGLE);*/
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect(elem, &rect);

        element_draw_rect(elem, &rect, windowTheme.background);
        rect.bottom = rect.top + windowTheme.edgeWidth;
        element_draw_rect(elem, &rect, windowTheme.bright);

        //win_timer_set(window, 0);
    }
    break;
    /*case LMSG_COMMAND:
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
    break;*/
    /*case LMSG_TIMER:
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
    break;*/
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_open();

    rect_t rect;
    display_screen_rect(disp, &rect, 0);
    rect.top = rect.bottom - TOPBAR_HEIGHT;
    window_t* win = window_new(disp, "Taskbar", &rect, SURFACE_PANEL, WINDOW_NONE, procedure);
    if (win == NULL)
    {
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_connected(disp))
    {
        display_next_event(disp, &event, NEVER);
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_close(disp);
    return 0;
}
