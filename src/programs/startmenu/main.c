#include <libdwm/dwm.h>
#include <stdio.h>

#define TOPBAR_HEIGHT 43

#define START_BUTTON_HEIGHT 32

#define START_MENU_WIDTH 250
#define START_MENU_HEIGHT 400

#define START_MENU_SHUT_DOWN_ID 100
#define START_MENU_RESTART_ID 101

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
    {.name = "Doom", .path = "home:/usr/bin/doom"},
    {.name = "Error Test", .path = "this:/is/a/nonsense/file/path"},
};

static uint64_t procedure(window_t* win, element_t* elem, const event_t* event)
{
    switch (event->type)
    {
    case LEVENT_INIT:
    {
        rect_t rect;
        element_content_rect(elem, &rect);

        for (uint64_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
        {
            rect_t buttonRect = RECT_INIT(windowTheme.edgeWidth * 2,
                windowTheme.edgeWidth + windowTheme.edgeWidth * (i + 1) + i * START_BUTTON_HEIGHT,
                RECT_WIDTH(&rect) - windowTheme.edgeWidth * 2,
                windowTheme.edgeWidth + windowTheme.edgeWidth * (i + 1) + i * START_BUTTON_HEIGHT +
                    START_BUTTON_HEIGHT);

            button_new(elem, i, &buttonRect, NULL, windowTheme.dark, windowTheme.background, BUTTON_NONE,
                entries[i].name);
        }

        /*rect_t shutdownRect = RECT_INIT();
        win_button_new(window, entries[i].name, &shutdownRect, START_MENU_SHUT_DOWN_ID, &props, WIN_BUTTON_NONE);

        rect_t restartRect = RECT_INIT()
        win_button_new(window, entries[i].name, &rect, START_MENU_RESTART_ID, &props, WIN_BUTTON_NONE);*/
    }
    break;
    case LEVENT_REDRAW:
    {
        rect_t rect;
        element_content_rect(elem, &rect);

        drawable_t* draw = element_draw(elem);

        draw_edge(draw, &rect, windowTheme.edgeWidth, windowTheme.bright, windowTheme.dark);
        RECT_SHRINK(&rect, windowTheme.edgeWidth);
        draw_rect(draw, &rect, windowTheme.background);
    }
    break;
    case LEVENT_ACTION:
    {
        if (event->lAction.type == ACTION_RELEASE)
        {
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
    }

    return 0;
}

int main(void)
{
    display_t* disp = display_new();

    rect_t screenRect;
    display_screen_rect(disp, &screenRect, 0);

    rect_t rect = RECT_INIT_DIM(0, RECT_HEIGHT(&screenRect) - TOPBAR_HEIGHT - START_MENU_HEIGHT, START_MENU_WIDTH,
        START_MENU_HEIGHT);
    window_t* win = window_new(disp, "StartMenu", &rect, SURFACE_WINDOW, WINDOW_NONE, procedure, NULL);

    event_t event = {0};
    while (display_connected(disp))
    {
        display_next_event(disp, &event, CLOCKS_NEVER);
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);
    return 0;
}
