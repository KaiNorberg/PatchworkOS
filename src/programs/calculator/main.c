#include <stdint.h>
#include <stdlib.h>
#include <sys/keyboard.h>
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

        rect_t rect;
        RECT_INIT(&rect, 5, 5, surface.width - 5, 5 + 64);

        gfx_rect(&surface, &rect, theme.background);
        gfx_edge(&surface, &rect, theme.edgeWidth, theme.shadow, theme.highlight);

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

int main(void)
{
    win_default_theme(&theme);

    rect_t rect;
    RECT_INIT_DIM(&rect, 500, 200, WINDOW_WIDTH, WINDOW_HEIGHT);
    win_client_to_window(&rect, &theme, WIN_WINDOW);

    win_t* window = win_new("Calculator", &rect, &theme, procedure, WIN_WINDOW);
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
