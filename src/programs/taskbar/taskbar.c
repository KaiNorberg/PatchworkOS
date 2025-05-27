#include "taskbar.h"

#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    taskbar_t* taskbar = element_private_get(elem);

    pixel_t background = element_color_get(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_NORMAL);
    pixel_t highlight = element_color_get(elem, COLOR_SET_DECO, COLOR_ROLE_HIGHLIGHT);
    int64_t panelSize = element_int_get(elem, INT_PANEL_SIZE);
    int64_t frameSize = element_int_get(elem, INT_FRAME_SIZE);
    int64_t smallPadding = element_int_get(elem, INT_SMALL_PADDING);
    int64_t bigPadding = element_int_get(elem, INT_BIG_PADDING);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        rect_t rect;
        element_content_rect_get(elem, &rect);

        rect_t startRect =
            RECT_INIT_DIM(bigPadding, frameSize + smallPadding, START_WIDTH, panelSize - frameSize - smallPadding * 2);
        button_new(elem, START_ID, &startRect, "Start", ELEMENT_TOGGLE | ELEMENT_NO_OUTLINE);

        rect_t clockRect = RECT_INIT_DIM(RECT_WIDTH(&rect) - CLOCK_WIDTH - bigPadding, frameSize + smallPadding,
            CLOCK_WIDTH, panelSize - frameSize - smallPadding * 2);

        element_t* clockLabel = label_new(elem, CLOCK_LABEL_ID, &clockRect, "0", ELEMENT_NONE);
        // element_color_set(clockLabel, COLOR_SET_VIEW, COLOR_ROLE_BACKGROUND_NORMAL, background);

        window_timer_set(win, TIMER_REPEAT, CLOCKS_PER_SEC * 30);
    }
    case EVENT_TIMER: // Fall trough
    {
        time_t epoch = time(NULL);
        struct tm timeData;
        localtime_r(&epoch, &timeData);
        char buffer[MAX_PATH];
        sprintf(buffer, "%02d:%02d %d-%02d-%02d", timeData.tm_hour, timeData.tm_min, timeData.tm_year + 1900,
            timeData.tm_mon + 1, timeData.tm_mday);
        element_t* clockLabel = element_find(elem, CLOCK_LABEL_ID);

        element_text_set(clockLabel, buffer);
        element_send_redraw(clockLabel, false);
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect_get(elem, &rect);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        draw_rect(&draw, &rect, background);

        rect.bottom = rect.top + frameSize;
        draw_rect(&draw, &rect, highlight);

        element_draw_end(elem, &draw);
    }
    break;
    case LEVENT_ACTION:
    {
        if (event->lAction.source == START_ID)
        {
            if (event->lAction.type == ACTION_PRESS)
            {
                start_menu_open(&taskbar->startMenu);
            }
            else if (event->lAction.type == ACTION_RELEASE)
            {
                start_menu_close(&taskbar->startMenu);
            }
        }
    }
    break;
    case UEVENT_START_MENU_CLOSE:
    {
        levent_force_action_t event;
        event.action = ACTION_RELEASE;
        element_emit(elem, LEVENT_FORCE_ACTION, &event, sizeof(event));
    }
    break;
    }

    return 0;
}

void taskbar_init(taskbar_t* taskbar, display_t* disp)
{
    rect_t rect;
    display_screen_rect(disp, &rect, 0);
    rect.top = rect.bottom - theme_int_get(INT_PANEL_SIZE, NULL);

    taskbar->disp = disp;
    taskbar->win = window_new(disp, "Taskbar", &rect, SURFACE_PANEL, WINDOW_NONE, procedure, taskbar);
    if (taskbar->win == NULL)
    {
        exit(EXIT_FAILURE);
    }
    start_menu_init(&taskbar->startMenu, taskbar->win, disp);
}

void taskbar_deinit(taskbar_t* taskbar)
{
    window_free(taskbar->win);
    start_menu_deinit(&taskbar->startMenu);
}