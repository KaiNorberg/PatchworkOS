#include "wallpaper.h"
#include "sys/win.h"

#include <stdlib.h>
#include <sys/proc.h>

static win_theme_t theme;

static uint64_t procedure(win_t* window, surface_t* surface, msg_t type, void* data)
{
    switch (type)
    {
    case LMSG_INIT:
    {
    }
    break;
    case LMSG_REDRAW:
    {
        rect_t rect = RECT_INIT_DIM(0, 0, surface->width, surface->height);

        gfx_rect(surface, &rect, 0xFF007E81);
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
