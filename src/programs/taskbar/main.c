#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

static uint64_t procedure(win_t* window, surface_t* surface, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_REDRAW:
    {
        win_theme_t theme;
        win_theme(window, &theme);

        rect_t rect;
        win_client_area(window, &rect);

        gfx_rect(surface, &rect, theme.background);

        rect.bottom = rect.top + theme.edgeWidth;
        gfx_rect(surface, &rect, theme.highlight);
    }
    break;
    }

    return 0;
}

int main(void)
{
    win_theme_t theme;
    win_default_theme(&theme);

    rect_t rect;
    win_screen_rect(&rect);
    rect.top = rect.bottom - 45;

    win_t* taskbar = win_new("Taskbar", &rect, &theme, procedure, WIN_PANEL);
    if (taskbar == NULL)
    {
        return EXIT_FAILURE;
    }

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(taskbar, &msg, NEVER);
        win_dispatch(taskbar, &msg);
    }

    win_free(taskbar);
    return 0;
}
