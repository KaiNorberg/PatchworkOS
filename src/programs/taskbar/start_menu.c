#include "start_menu.h"

#include "taskbar.h"

#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>

typedef struct
{
    const char* name;
    const char* path;
} start_entry_t;

// TODO: Load this from config file.
static start_entry_t entries[] = {
    {.name = "Calculator", .path = "/usr/bin/calculator"},
    {.name = "Terminal", .path = "/usr/bin/terminal"},
    {.name = "Tetris", .path = "/usr/bin/tetris"},
    {.name = "DOOM", .path = "/usr/bin/doom"},
};

#define ENTRY_AMOUNT (sizeof(entries) / sizeof(entries[0]))

#define START_MENU_HEIGHT(frameSize) (frameSize + 12 * START_BUTTON_HEIGHT)

#define START_MENU_YPOS_START(screenRect, panelSize, frameSize) (RECT_HEIGHT(screenRect))
#define START_MENU_YPOS_END(screenRect, panelSize, frameSize) \
    (RECT_HEIGHT(screenRect) - START_MENU_HEIGHT(frameSize) - panelSize)

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    start_menu_t* startMenu = element_get_private(elem);
    const theme_t* theme = element_get_theme(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        rect_t rect = element_get_content_rect(elem);

        for (uint64_t i = 0; i < ENTRY_AMOUNT; i++)
        {
            rect_t buttonRect =
                RECT_INIT(theme->frameSize + theme->titlebarSize, theme->frameSize + i * START_BUTTON_HEIGHT,
                    RECT_WIDTH(&rect) - theme->frameSize, (i + 1) * START_BUTTON_HEIGHT);

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

        draw_frame(&draw, &rect, theme->frameSize, theme->deco.highlight, theme->deco.shadow);
        RECT_SHRINK(&rect, theme->frameSize);
        draw_rect(&draw, &rect, theme->deco.backgroundNormal);

        rect.right = rect.left + theme->titlebarSize;
        draw_gradient(&draw, &rect, theme->deco.backgroundSelectedStart, theme->deco.backgroundSelectedEnd,
            DIRECTION_VERTICAL, false);

        element_draw_end(elem, &draw);
    }
    break;
    case LEVENT_ACTION:
    {
        if (event->lAction.type == ACTION_RELEASE)
        {
            start_menu_close(startMenu);

            fd_t klog = open("/dev/klog");

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

        int32_t startY = START_MENU_YPOS_START(&screenRect, theme->panelSize, theme->frameSize);
        int32_t endY = START_MENU_YPOS_END(&screenRect, theme->panelSize, theme->frameSize);

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
    const theme_t* theme = theme_global_get();
    if (theme == NULL)
    {
        printf("taskbar: failed to get global theme for start menu\n");
        abort();
    }

    rect_t screenRect;
    display_screen_rect(disp, &screenRect, 0);

    rect_t rect =
        RECT_INIT_DIM(theme->smallPadding, START_MENU_YPOS_START(&screenRect, theme->panelSize, theme->frameSize),
            START_MENU_WIDTH, START_MENU_HEIGHT(theme->frameSize));

    startMenu->taskbar = taskbar;
    startMenu->win = window_new(disp, "StartMenu", &rect, SURFACE_WINDOW, WINDOW_NONE, procedure, startMenu);
    if (startMenu->win == NULL)
    {
        printf("tasbar: failed to create start menu window\n");
        abort();
    }
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
    const theme_t* theme = element_get_theme(elem);

    display_screen_rect(window_get_display(startMenu->win), &screenRect, 0);

    int32_t startY = START_MENU_YPOS_START(&screenRect, theme->panelSize, theme->frameSize);

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
