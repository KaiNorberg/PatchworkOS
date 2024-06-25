#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

static win_theme_t theme;

static uint64_t procedure(win_t* window, msg_t type, void* data)
{
    switch (type)
    {
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
    }

    return 0;
}

int main(void)
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

    while (win_dispatch(taskbar, NEVER) != LMSG_QUIT)
    {
    }

    win_free(taskbar);

    return 0;
}
