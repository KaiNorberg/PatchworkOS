#include "start_menu.h"

#include <libdwm/dwm.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TOPBAR_HEIGHT 43
#define TOPBAR_PADDING 5

#define START_WIDTH 75
#define START_ID 0

#define CLOCK_WIDTH 150

#define UEVENT_CLOCK (UEVENT_BASE + 1)

#define CLOCK_LABEL_ID 1234

static label_t* clockLabel;

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case LEVENT_INIT:
    {
        rect_t rect;
        element_content_rect(elem, &rect);

        rect_t startRect = RECT_INIT_DIM(TOPBAR_PADDING, TOPBAR_PADDING + windowTheme.edgeWidth, START_WIDTH,
            TOPBAR_HEIGHT - TOPBAR_PADDING * 2);
        button_new(elem, START_ID, &startRect, NULL, windowTheme.dark, windowTheme.background, BUTTON_TOGGLE, "Start");

        rect_t clockRect = RECT_INIT_DIM(RECT_WIDTH(&rect) - TOPBAR_PADDING - CLOCK_WIDTH,
            TOPBAR_PADDING + windowTheme.edgeWidth, CLOCK_WIDTH, TOPBAR_HEIGHT - TOPBAR_PADDING * 2);
        clockLabel = label_new(elem, CLOCK_LABEL_ID, &clockRect, NULL, ALIGN_CENTER, ALIGN_CENTER, windowTheme.dark,
            windowTheme.background, LABEL_NONE, "0");
    }
    case UEVENT_CLOCK: // Fall trough
    {
        time_t epoch = time(NULL);
        struct tm timeData;
        localtime_r(&epoch, &timeData);
        char buffer[MAX_PATH];
        sprintf(buffer, "%02d:%02d %d-%02d-%02d", timeData.tm_hour, timeData.tm_min, timeData.tm_year + 1900,
            timeData.tm_mon + 1, timeData.tm_mday);
        label_set_text(clockLabel, buffer);
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect(elem, &rect);

        drawable_t* draw = element_draw(elem);

        draw_rect(draw, &rect, windowTheme.background);
        rect.bottom = rect.top + windowTheme.edgeWidth;
        draw_rect(draw, &rect, windowTheme.bright);
    }
    break;
    case LEVENT_ACTION:
    {
        if (event->lAction.source == START_ID)
        {
            if (event->lAction.type == ACTION_PRESS)
            {
                start_menu_open();
            }
            else if (event->lAction.type == ACTION_RELEASE)
            {
                start_menu_close();
            }
        }
    }
    break;
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_new();

    rect_t rect;
    display_screen_rect(disp, &rect, 0);
    rect.top = rect.bottom - TOPBAR_HEIGHT;
    window_t* win = window_new(disp, "Taskbar", &rect, SURFACE_PANEL, WINDOW_NONE, procedure, NULL);
    if (win == NULL)
    {
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_connected(disp))
    {
        if (display_next_event(disp, &event, CLOCKS_PER_SEC * 60))
        {
            display_dispatch(disp, &event);
        }
        else
        {
            display_emit(disp, window_id(win), UEVENT_CLOCK, NULL, 0);
        }
    }

    window_free(win);
    display_free(disp);
    return 0;
}
