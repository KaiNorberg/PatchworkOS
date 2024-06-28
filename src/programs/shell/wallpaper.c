#include "wallpaper.h"
#include "sys/win.h"

#include <stdlib.h>
#include <sys/proc.h>

static win_theme_t theme;

uint64_t procedure(win_t* window, msg_t type, void* data)
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

        rect_t rect = RECT_INIT_DIM(0, 0, surface.width, surface.height);

        gfx_rect(&surface, &rect, 0xFF007E81);

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

win_t* wallpaper_init(void)
{
    win_default_theme(&theme);

    rect_t rect;
    win_screen_rect(&rect);

    win_t* wallpaper = win_new("Wallpaper", &rect, &theme, procedure, WIN_WALL);
    if (wallpaper == NULL)
    {
        exit(EXIT_FAILURE);
    }

    return wallpaper;
}
