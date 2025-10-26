#include "start_menu.h"

#include "taskbar.h"

#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <string.h>
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

static uint64_t startmenu_procedure(window_t* win, element_t* elem, const event_t* event)
{
    const theme_t* theme = element_get_theme(elem);

    switch (event->type)
    {
    case LEVENT_INIT:
    {
        start_menu_t* menu = malloc(sizeof(start_menu_t));
        if (menu == NULL)
        {
            printf("startmenu: failed to allocate start menu private data\n");
            errno = ENOMEM;
            return ERR;
        }
        menu->win = win;
        menu->taskbar = element_get_private(elem);
        menu->animationStartTime = uptime();
        menu->state = START_MENU_CLOSED;

        rect_t rect = element_get_content_rect(elem);

        for (uint64_t i = 0; i < ENTRY_AMOUNT; i++)
        {
            rect_t buttonRect =
                RECT_INIT(theme->frameSize + theme->titlebarSize, theme->frameSize + i * START_BUTTON_HEIGHT,
                    RECT_WIDTH(&rect) - theme->frameSize, (i + 1) * START_BUTTON_HEIGHT);

            button_new(elem, i, &buttonRect, entries[i].name, ELEMENT_FLAT);
        }

        element_set_private(elem, menu);
    }
    break;
    case LEVENT_DEINIT:
    {
        start_menu_t* menu = element_get_private(elem);
        if (menu == NULL)
        {
            break;
        }
        free(menu);
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
        start_menu_t* menu = element_get_private(elem);
        if (event->lAction.type == ACTION_RELEASE)
        {
            start_menu_close(win);

            const char* argv[] = {entries[event->lAction.source].path, NULL};
            if (spawn(argv, NULL, NULL, NULL) == ERR)
            {
                char buffer[MAX_PATH];
                sprintf(buffer, "Failed to spawn (%s)!", entries[event->lAction.source].path);

                popup_open(buffer, "Error!", POPUP_OK);
            }
        }
    }
    break;
    case EVENT_TIMER:
    {
        start_menu_t* menu = element_get_private(elem);
        rect_t screenRect = display_screen_rect(window_get_display(win), 0);

        int32_t startY = START_MENU_YPOS_START(&screenRect, theme->panelSize, theme->frameSize);
        int32_t endY = START_MENU_YPOS_END(&screenRect, theme->panelSize, theme->frameSize);

        clock_t timeElapsed = uptime() - menu->animationStartTime;

        double fraction;
        int64_t currentY = 0;
        bool isAnimComplete = false;

        if (menu->state == START_MENU_OPENING)
        {
            fraction = (double)timeElapsed / START_MENU_ANIMATION_TIME;
            if (fraction >= 1.0)
            {
                fraction = 1.0;
                isAnimComplete = true;
            }
            currentY = (int64_t)((double)startY + ((double)endY - startY) * fraction);
        }
        else if (menu->state == START_MENU_CLOSING)
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
            return 0;
        }

        rect_t rect = window_get_rect(win);
        uint64_t height = RECT_HEIGHT(&rect);
        rect.top = currentY;
        rect.bottom = currentY + height;
        if (window_move(win, &rect) == ERR)
        {
            printf("startmenu: failed to move window during animation (%s)\n", strerror(errno));
        }

        if (isAnimComplete)
        {
            if (window_set_timer(win, TIMER_NONE, CLOCKS_NEVER) == ERR)
            {
                printf("startmenu: failed to disable timer (%s)\n", strerror(errno));
            }
            if (menu->state == START_MENU_OPENING)
            {
                menu->state = START_MENU_OPEN;
            }
            else if (menu->state == START_MENU_CLOSING)
            {
                menu->state = START_MENU_CLOSED;
            }
        }
    }
    break;
    }

    return 0;
}

window_t* start_menu_new(window_t* taskbar, display_t* disp)
{
    const theme_t* theme = theme_global_get();
    rect_t screenRect = display_screen_rect(disp, 0);

    rect_t rect =
        RECT_INIT_DIM(theme->smallPadding, START_MENU_YPOS_START(&screenRect, theme->panelSize, theme->frameSize),
            START_MENU_WIDTH, START_MENU_HEIGHT(theme->frameSize));

    window_t* win = window_new(disp, "StartMenu", &rect, SURFACE_WINDOW, WINDOW_NONE, startmenu_procedure, taskbar);
    if (win == NULL)
    {
        printf("startmenu: failed to create start menu window\n");
        return NULL;
    }

    if (window_set_visible(win, true) == ERR)
    {
        window_free(win);
        return NULL;
    }

    return win;
}

void start_menu_open(window_t* startMenu)
{
    element_t* elem = window_get_client_element(startMenu);
    const theme_t* theme = element_get_theme(elem);
    start_menu_t* menu = element_get_private(elem);

    if (menu->state == START_MENU_OPEN || menu->state == START_MENU_OPENING)
    {
        return;
    }

    rect_t screenRect = display_screen_rect(window_get_display(startMenu), 0);

    int32_t startY = START_MENU_YPOS_START(&screenRect, theme->panelSize, theme->frameSize);

    rect_t rect = window_get_rect(startMenu);
    uint64_t height = RECT_HEIGHT(&rect);
    rect.top = startY;
    rect.bottom = startY + height;
    window_move(startMenu, &rect);

    menu->animationStartTime = uptime();
    menu->state = START_MENU_OPENING;
    window_set_timer(startMenu, TIMER_REPEAT, CLOCKS_PER_SEC / 60);

    window_set_focus(startMenu);
}

void start_menu_close(window_t* startMenu)
{
    element_t* elem = window_get_client_element(startMenu);
    start_menu_t* menu = element_get_private(elem);
    if (menu->state == START_MENU_CLOSED || menu->state == START_MENU_CLOSING)
    {
        return;
    }

    menu->animationStartTime = uptime();
    menu->state = START_MENU_CLOSING;
    window_set_timer(startMenu, TIMER_REPEAT, CLOCKS_PER_SEC / 60);

    display_events_push(window_get_display(startMenu), window_get_id(menu->taskbar), UEVENT_START_MENU_CLOSE,
        NULL, 0);
}

start_menu_state_t start_menu_get_state(window_t* startMenu)
{
    if (startMenu == NULL)
    {
        return START_MENU_CLOSED;
    }

    element_t* elem = window_get_client_element(startMenu);
    start_menu_t* menu = element_get_private(elem);
    return menu->state;
}
