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
        data->name = "Wallpaper";
        data->type = DWM_WALL;
        win_screen_rect(&data->rect);
    }
    break;
    case LMSG_REDRAW:
    {
        rect_t rect = RECT_INIT_SURFACE(surface);
        gfx_rect(surface, &rect, 0xFF007E81);
    }
    break;
    }

    return 0;
}

int main(void)
{
    win_t* wallpaper = win_new(procedure);
    if (wallpaper == NULL)
    {
        return EXIT_FAILURE;
    }

    // Wait for wallpaper to be drawn before spawning processes.
    msg_t msg = {0};
    while (msg.type != LMSG_QUIT && msg.type != LMSG_REDRAW)
    {
        win_receive(wallpaper, &msg, NEVER);
        win_dispatch(wallpaper, &msg);
    }

    spawn("/bin/cursor.elf");
    spawn("/bin/taskbar.elf");

    spawn("/bin/calculator.elf");
    spawn("/bin/calculator.elf");
    spawn("/bin/calculator.elf");
    spawn("/bin/calculator.elf");

    msg = (msg_t){0};
    while (msg.type != LMSG_QUIT)
    {
        win_receive(wallpaper, &msg, NEVER);
        win_dispatch(wallpaper, &msg);
    }

    win_free(wallpaper);
    return EXIT_SUCCESS;
}
