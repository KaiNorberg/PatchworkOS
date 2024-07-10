#include <stdint.h>
#include <stdlib.h>
#include <sys/keyboard.h>
#include <sys/proc.h>
#include <sys/win.h>

#define WINDOW_WIDTH 350
#define WINDOW_HEIGHT 400

static uint64_t procedure(win_t* window, surface_t* surface, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_REDRAW:
    {
        win_theme_t theme;
        win_theme(window, &theme);

        rect_t rect = RECT_INIT(5, 5, surface->width - 5, 5 + 40);
        gfx_rect(surface, &rect, theme.background);
        gfx_edge(surface, &rect, theme.edgeWidth, theme.shadow, theme.highlight);
    }
    break;
    }

    return 0;
}

int main(void)
{
    win_theme_t theme;
    win_default_theme(&theme);

    rect_t rect = RECT_INIT_DIM(500 * (1 + getpid() % 2), 200, WINDOW_WIDTH, WINDOW_HEIGHT);
    win_client_to_window(&rect, &theme, WIN_WINDOW);

    win_t* window = win_new("Calculator", &rect, &theme, procedure, WIN_WINDOW);
    if (window == NULL)
    {
        return EXIT_FAILURE;
    }

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(window, &msg, NEVER);
        win_dispatch(window, &msg);
    }

    win_free(window);
    return EXIT_SUCCESS;
}
