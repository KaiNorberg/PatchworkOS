#include "sys/gfx.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/kbd.h>
#include <sys/proc.h>
#include <sys/win.h>

#define WINDOW_WIDTH 350
#define WINDOW_HEIGHT 500

win_theme_t theme;

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

        rect_t rect = (rect_t){
            .left = 5,
            .top = 5,
            .right = surface.width - 5,
            .bottom = 5 + 64,
        };

        gfx_rect(&surface, &rect, theme.background);
        gfx_edge(&surface, &rect, theme.edgeWidth, theme.shadow, theme.highlight);
    }
    break;
    case LMSG_QUIT:
    {
    }
    break;
    }

    return 0;
}

int main(void)
{
    win_default_theme(&theme);

    rect_t rect = (rect_t){
        .left = 500,
        .top = 200,
        .right = 500 + WINDOW_WIDTH,
        .bottom = 200 + WINDOW_HEIGHT,
    };
    win_client_to_window(&rect, &theme, WIN_DECO);

    win_t* window = win_new("Calculator", &rect, &theme, procedure, WIN_DECO);
    if (window == NULL)
    {
        exit(EXIT_FAILURE);
    }

    while (win_dispatch(window, NEVER) != LMSG_QUIT)
    {
    }

    win_free(window);

    return EXIT_SUCCESS;
}
