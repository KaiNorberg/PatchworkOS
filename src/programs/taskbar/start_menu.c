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
    start_menu_t* startMenu = element_private_get(elem);

    int64_t frameSize = element_int_get(elem, INT_FRAME_SIZE);
    int64_t titlebarSize = element_int_get(elem, INT_TITLEBAR_SIZE);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        rect_t rect;
        element_content_rect_get(elem, &rect);

        for (uint64_t i = 0; i < ENTRY_AMOUNT; i++)
        {
            rect_t buttonRect = RECT_INIT(frameSize + titlebarSize, frameSize + i * START_BUTTON_HEIGHT,
                RECT_WIDTH(&rect) - frameSize, (i + 1) * START_BUTTON_HEIGHT);

            button_new(elem, i, &buttonRect, entries[i].name, ELEMENT_FLAT);
        }

        window_timer_set(win, TIMER_REPEAT, CLOCKS_PER_SEC / 60);
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect_get(elem, &rect);

        drawable_t draw;
        element_draw_begin(elem, &draw);

        int64_t frameSize = element_int_get(elem, INT_FRAME_SIZE);
        pixel_t highlight = element_color_get(elem, COLOR_SET_DECO, COLOR_ROLE_HIGHLIGHT);
        pixel_t shadow = element_color_get(elem, COLOR_SET_DECO, COLOR_ROLE_SHADOW);
        pixel_t background = element_color_get(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_NORMAL);
        pixel_t selectedStart = element_color_get(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_SELECTED_START);
        pixel_t selectedEnd = element_color_get(elem, COLOR_SET_DECO, COLOR_ROLE_BACKGROUND_SELECTED_END);

        draw_frame(&draw, &rect, frameSize, highlight, shadow);
        RECT_SHRINK(&rect, frameSize);
        draw_rect(&draw, &rect, background);

        rect.right = rect.left + titlebarSize;
        draw_gradient(&draw, &rect, selectedStart, selectedEnd, GRADIENT_VERTICAL, false);

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
            if (spawn(argv, fds) == ERR)
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
        if (startMenu->focusOutPending)
        {
            clock_t elapsed = uptime() - startMenu->focusOutTime;
            if (elapsed >= CLOCKS_PER_SEC / 30)
            {
                startMenu->focusOutPending = false;
                start_menu_close(startMenu);
            }
            break;
        }

        rect_t screenRect;
        display_screen_rect(window_display_get(win), &screenRect, 0);

        int64_t panelSize = element_int_get(elem, INT_PANEL_SIZE);
        int64_t frameSize = element_int_get(elem, INT_FRAME_SIZE);

        int32_t startY = START_MENU_YPOS_START(&screenRect, panelSize, frameSize);
        int32_t endY = START_MENU_YPOS_END(&screenRect, panelSize, frameSize);

        clock_t timeElapsed = uptime() - startMenu->animationStartTime;

        double fraction;
        int64_t currentY = 0;
        bool animation_complete = false;

        if (startMenu->state == START_MENU_OPENING)
        {
            fraction = (double)timeElapsed / START_MENU_ANIMATION_TIME;
            if (fraction >= 1.0)
            {
                fraction = 1.0;
                animation_complete = true;
            }
            currentY = (int64_t)((double)startY + ((double)endY - startY) * fraction);
        }
        else if (startMenu->state == START_MENU_CLOSING)
        {
            fraction = (double)timeElapsed / START_MENU_ANIMATION_TIME;
            if (fraction >= 1.0)
            {
                fraction = 1.0;
                animation_complete = true;
            }
            currentY = (int64_t)((double)endY + ((double)startY - endY) * fraction);
        }
        else
        {

            window_timer_set(win, TIMER_NONE, CLOCKS_NEVER);
            return 0;
        }

        rect_t rect;
        window_rect_get(win, &rect);
        uint64_t height = RECT_HEIGHT(&rect);
        rect.top = currentY;
        rect.bottom = currentY + height;
        window_move(win, &rect);

        if (animation_complete)
        {
            window_timer_set(win, TIMER_NONE, CLOCKS_NEVER);
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
    case EVENT_FOCUS_OUT:
    {
        // This is a super hacky fix, but basically, if the start menu is opened and then closed by pressing the start button while the start_menu still has focus then
        // the EVENT_FOCUS_OUT will fire when the button is pressed (shifting focus from start_menu to taskbar), which will close the menu before the ACTION_RELEASE
        // event can be processed. This means when ACTION_RELEASE finally fires, it sees a closed menu and tries to open it again, causing the menu to briefly close
        // and immediately reopen. This fixes that.
        startMenu->focusOutPending = true;
        startMenu->focusOutTime = uptime();
        window_timer_set(win, TIMER_REPEAT, CLOCKS_PER_SEC / 60);
    
    }
    break;
    }

    return 0;
}

void start_menu_init(start_menu_t* startMenu, window_t* taskbar, display_t* disp)
{
    int64_t panelSize = theme_int_get(INT_PANEL_SIZE, NULL);
    int64_t frameSize = theme_int_get(INT_FRAME_SIZE, NULL);
    int64_t smallPadding = theme_int_get(INT_SMALL_PADDING, NULL);

    rect_t screenRect;
    display_screen_rect(disp, &screenRect, 0);

    rect_t rect = RECT_INIT_DIM(smallPadding, START_MENU_YPOS_START(&screenRect, panelSize, frameSize),
        START_MENU_WIDTH, START_MENU_HEIGHT(frameSize));

    startMenu->taskbar = taskbar;
    startMenu->win = window_new(disp, "StartMenu", &rect, SURFACE_WINDOW, WINDOW_NONE, procedure, startMenu);
    startMenu->state = START_MENU_CLOSED;
    startMenu->focusOutPending = false;
    startMenu->focusOutTime = 0;
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

    element_t* elem = window_client_element_get(startMenu->win);

    display_screen_rect(window_display_get(startMenu->win), &screenRect, 0);
    int64_t panelSize = element_int_get(elem, INT_PANEL_SIZE);
    int64_t frameSize = element_int_get(elem, INT_FRAME_SIZE);

    int32_t startY = START_MENU_YPOS_START(&screenRect, panelSize, frameSize);

    rect_t rect;
    window_rect_get(startMenu->win, &rect);
    uint64_t height = RECT_HEIGHT(&rect);
    rect.top = startY;
    rect.bottom = startY + height;
    window_move(startMenu->win, &rect);

    startMenu->animationStartTime = uptime();
    startMenu->state = START_MENU_OPENING;
    window_timer_set(startMenu->win, TIMER_REPEAT, CLOCKS_PER_SEC / 60);

    window_focus_set(startMenu->win);
}

void start_menu_close(start_menu_t* startMenu)
{
    if (startMenu->state == START_MENU_CLOSED || startMenu->state == START_MENU_CLOSING)
    {
        return;
    }

    startMenu->animationStartTime = uptime();
    startMenu->state = START_MENU_CLOSING;
    window_timer_set(startMenu->win, TIMER_REPEAT, CLOCKS_PER_SEC / 60);

    display_events_push(window_display_get(startMenu->win), window_id_get(startMenu->taskbar), UEVENT_START_MENU_CLOSE,
        NULL, 0);
}