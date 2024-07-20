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
        gfx_t gfx;
        win_draw_begin(window, &gfx);

        win_theme_t theme;
        win_theme(&theme);
        rect_t rect;
        win_client_rect(window, &rect);

        gfx_rect(&gfx, &rect, theme.background);
        rect.bottom = rect.top + theme.edgeWidth;
        gfx_rect(&gfx, &rect, theme.bright);

        win_draw_end(window, &gfx);
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

    win_t* panel = win_new("Taskbar", &rect, DWM_PANEL, WIN_NONE, procedure);
    if (panel == NULL)
    {
        return EXIT_FAILURE;
    }

    msg_t msg = {0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(panel, &msg, NEVER);
        win_dispatch(panel, &msg);
    }

    win_free(panel);
    return 0;
}
