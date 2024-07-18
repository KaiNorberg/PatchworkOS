#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

static uint64_t procedure(win_t* window, const msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_REDRAW:
    {
        surface_t surface;
        win_draw_begin(window, &surface);

        win_theme_t theme;
        win_theme(&theme);
        rect_t rect;
        win_client_area(window, &rect);

        gfx_rect(&surface, &rect, theme.background);
        rect.bottom = rect.top + theme.edgeWidth;
        gfx_rect(&surface, &rect, theme.bright);

        win_draw_end(window, &surface);
    }
    break;
    }

    return 0;
}

int main(void)
{
    rect_t rect;
    win_screen_rect(&rect);
    rect.top = rect.bottom - 45;

    win_t* window = win_new("Taskbar", DWM_PANEL, &rect, procedure);
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
    return 0;
}
