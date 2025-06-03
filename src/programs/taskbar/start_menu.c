#include "start_menu.h"

#include "taskbar.h"

#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <sys/io.h>
#include <sys/proc.h>

typedef struct
{
    const char* name;
    const char* path;
} start_entry_t;

// TODO: Load this from config file.
static start_entry_t entries[] = {
    {.name = "Calculator", .path = "home:/usr/bin/calculator"},
    {.name = "Terminal", .path = "home:/usr/bin/terminal"},
    {.name = "Tetris", .path = "home:/usr/bin/tetris"},
    {.name = "DOOM", .path = "home:/usr/bin/doom"},
};

#define ENTRY_AMOUNT (sizeof(entries) / sizeof(entries[0]))

#define START_MENU_HEIGHT(frameSize) (frameSize + 12 * START_BUTTON_HEIGHT)

#define START_MENU_YPOS_START(screenRect, panelSize, frameSize) (RECT_HEIGHT(screenRect))
#define START_MENU_YPOS_END(screenRect, panelSize, frameSize) \
    (RECT_HEIGHT(screenRect) - START_MENU_HEIGHT(frameSize) - panelSize)

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    start_menu_t* startMenu = element_get_private(elem);

    int64_t frameSize = element_get_int(elem, INT_FRAME_SIZE);
    int64_t titlebarSize = element_get_int(elem, INT_TITLEBAR_SIZE);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        rect_t rect = element_get_content_rect(elem);

        for (uint64_t i = 0; i < ENTRY_AMOUNT; i++)
        {
            rect_t buttonRect = RECT_INIT(frameSize + titlebarSize, frameSize + i * START_BUTTON_HEIGHT,
                RECT_WIDTH(&rect) - frameSize, (i + 1) * START_BUTTON_HEIGHT);

            button_new(elem, i, &buttonRect, entries[i].name, ELEMENT_FLAT);
        }

        window_set_timer(win, TIMER_REPEAT, CLOCKS_PER_SEC / 60);
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect = element_get_content_rect(elem);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        int64_t frameSize = element_get_int(elem, INT_FRAME_SIZE);
        pixel_t highlight = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_HIGHLIGHT);
        pixel_t shadow = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_SHADOW);
        pixel_t background = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_NORMAL);
        pixel_t selectedStart = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_SELECTED_START);
        pixel_t selectedEnd = element_get_color(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_SELECTED_END);

        draw_frame(&draw, &rect, frameSize, highlight, shadow);
        RECT_SHRINK(&rect, frameSize);
        draw_rect(&draw, &rect, background);

        rect.right = rect.left + titlebarSize;
        draw_gradient(&draw, &rect, selectedStart, selectedEnd, DIRECTION_VERTICAL, false);

        element_draw_end(elem, &draw);
    }
    break;
    case LEVENT_ACTION:
    {
        if (event->lAction.type == ACTION_RELEASE)
        {
            start_menu_close(startMenu);

            fd_t klog = open("sys:/klog");

            const char* argv[] = {entries[event->lAction.source].path, NULL};
            spawn_fd_t fds[] = {{.child = STDOUT_FILENO, .parent = klog}, {.child = STDERR_FILENO, .parent = klog},
                SPAWN_FD_END};
            if (spawn(argv, fds, NULL, NULL) == ERR)
            {
                char buffer[MAX_PATH];
                sprintf(buffer, "Failed to spawn (%s)!", entries[event->lAction.source].path);

                popup_open(buffer, "Error!", POPUP_OK);
            }

            close(klog);
        }
    }
    break;
    case EVENT_TIMER:
    {
        rect_t screenRect;
        display_screen_rect(window_get_display(win), &screenRect, 0);

        int64_t panelSize = element_get_int(elem, INT_PANEL_SIZE);
        int64_t frameSize = element_get_int(elem, INT_FRAME_SIZE);

        int32_t startY = START_MENU_YPOS_START(&screenRect, panelSize, frameSize);
        int32_t endY = START_MENU_YPOS_END(&screenRect, panelSize, frameSize);

        clock_t timeElapsed = uptime() - startMenu->animationStartTime;

        double fraction;
        int64_t currentY = 0;
        bool isAnimComplete = false;

        if (startMenu->state == START_MENU_OPENING)
        {
            fraction = (double)timeElapsed / START_MENU_ANIMATION_TIME;
            if (fraction >= 1.0)
            {
                fraction = 1.0;
                isAnimComplete = true;
            }
            currentY = (int64_t)((double)startY + ((double)endY - startY) * fraction);
        }
        else if (startMenu->state == START_MENU_CLOSING)
        {
            fraction = (double)timeElapsed / START_MENU_ANIMATION_TIME;
            if (fraction >= 1.0)
            {
                fraction = 1.0;
                isAnimComplete = true;
            }
            currentY = (int64_t)((double)endY + ((double)startY - endY) * fraction);
        }
        else
        {

            window_set_timer(win, TIMER_NONE, CLOCKS_NEVER);
            return 0;
        }

        rect_t rect;
        window_get_rect(win, &rect);
        uint64_t height = RECT_HEIGHT(&rect);
        rect.top = currentY;
        rect.bottom = currentY + height;
        window_move(win, &rect);

        if (isAnimComplete)
        {
            window_set_timer(win, TIMER_NONE, CLOCKS_NEVER);
            if (startMenu->state == START_MENU_OPENING)
            {
                startMenu->state = START_MENU_OPEN;
            }
            else if (startMenu->state == START_MENU_CLOSING)
            {
                startMenu->state = START_MENU_CLOSED;
            }
        }
    }
    break;
    }

    return 0;
}

void start_menu_init(start_menu_t* startMenu, window_t* taskbar, display_t* disp)
{
    int64_t panelSize = theme_get_int(INT_PANEL_SIZE, NULL);
    int64_t frameSize = theme_get_int(INT_FRAME_SIZE, NULL);
    int64_t smallPadding = theme_get_int(INT_SMALL_PADDING, NULL);

    rect_t screenRect;
    display_screen_rect(disp, &screenRect, 0);

    rect_t rect = RECT_INIT_DIM(smallPadding, START_MENU_YPOS_START(&screenRect, panelSize, frameSize),
        START_MENU_WIDTH, START_MENU_HEIGHT(frameSize));

    startMenu->taskbar = taskbar;
    startMenu->win = window_new(disp, "StartMenu", &rect, SURFACE_WINDOW, WINDOW_NONE, procedure, startMenu);
    startMenu->state = START_MENU_CLOSED;
}

void start_menu_deinit(start_menu_t* startMenu)
{
    window_free(startMenu->win);
}

void start_menu_open(start_menu_t* startMenu)
{
    if (startMenu->state == START_MENU_OPEN || startMenu->state == START_MENU_OPENING)
    {
        return;
    }

    rect_t screenRect;

    element_t* elem = window_get_client_element(startMenu->win);

    display_screen_rect(window_get_display(startMenu->win), &screenRect, 0);
    int64_t panelSize = element_get_int(elem, INT_PANEL_SIZE);
    int64_t frameSize = element_get_int(elem, INT_FRAME_SIZE);

    int32_t startY = START_MENU_YPOS_START(&screenRect, panelSize, frameSize);

    rect_t rect;
    window_get_rect(startMenu->win, &rect);
    uint64_t height = RECT_HEIGHT(&rect);
    rect.top = startY;
    rect.bottom = startY + height;
    window_move(startMenu->win, &rect);

    startMenu->animationStartTime = uptime();
    startMenu->state = START_MENU_OPENING;
    window_set_timer(startMenu->win, TIMER_REPEAT, CLOCKS_PER_SEC / 60);

    window_set_focus(startMenu->win);
}

void start_menu_close(start_menu_t* startMenu)
{
    if (startMenu->state == START_MENU_CLOSED || startMenu->state == START_MENU_CLOSING)
    {
        return;
    }

    startMenu->animationStartTime = uptime();
    startMenu->state = START_MENU_CLOSING;
    window_set_timer(startMenu->win, TIMER_REPEAT, CLOCKS_PER_SEC / 60);

    display_events_push(window_get_display(startMenu->win), window_get_id(startMenu->taskbar), UEVENT_START_MENU_CLOSE,
        NULL, 0);
}
