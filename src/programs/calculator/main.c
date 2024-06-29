#include <stdint.h>
#include <stdlib.h>
#include <sys/keyboard.h>
#include <sys/proc.h>
#include <sys/win.h>

#define WINDOW_WIDTH 350
#define WINDOW_HEIGHT 400

win_theme_t theme;

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
        rect_t rect = RECT_INIT(5, 5, surface->width - 5, 5 + 40);
        gfx_rect(surface, &rect, theme.background);
        gfx_edge(surface, &rect, theme.edgeWidth, theme.shadow, theme.highlight);
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

    rect_t rect = RECT_INIT_DIM(500 * (1 + getpid() % 2), 200, WINDOW_WIDTH, WINDOW_HEIGHT);
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
