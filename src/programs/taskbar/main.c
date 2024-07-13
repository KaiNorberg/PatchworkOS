#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

static uint64_t procedure(win_t* window, void* private, surface_t* surface, msg_t* msg)
{
    switch (msg->type)
    {
    case LMSG_INIT:
    {
        lmsg_init_t* data = (lmsg_init_t*)msg->data;
        data->name = "Taskbar";
        data->type = DWM_PANEL;
        win_screen_rect(&data->rect);
        data->rect.top = data->rect.bottom - 45;
    }
    break;
    case LMSG_REDRAW:
    {
        win_theme_t theme;
        win_theme(&theme);
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
    win_t* taskbar = win_new(procedure);
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
