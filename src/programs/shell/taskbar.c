#include "taskbar.h"
#include "sys/win.h"

#include <stdlib.h>

static win_theme_t theme;

static uint64_t procedure(win_t* window, msg_t type, void* data)
{
    switch (type)
    {
    case LMSG_INIT:
    {
    }
    break;
    case LMSG_REDRAW:
    {
        surface_t surface;
        win_client_surface(window, &surface);

        rect_t rect;
        win_client_area(window, &rect);

        gfx_rect(&surface, &rect, theme.background);

        rect.bottom = rect.top + theme.edgeWidth;
        gfx_rect(&surface, &rect, theme.highlight);

        win_flush(window, &surface);
    }
    break;
    case LMSG_QUIT:
    {
    }
    break;
    }

    return 0;
}

win_t* taskbar_init(void)
{
    win_default_theme(&theme);

    rect_t rect;
    win_screen_rect(&rect);
    rect.top = rect.bottom - 45;

    win_t* taskbar = win_new("Taskbar", &rect, &theme, procedure, WIN_PANEL);
    if (taskbar == NULL)
    {
        exit(EXIT_FAILURE);
    }

    return taskbar;
}
