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

        rect_t rect = RECT_INIT_SURFACE(&surface);
        gfx_rect(&surface, &rect, 0xFF007E81);

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
    win_t* wallpaper = win_new("Wallpaper", DWM_WALL, &rect, procedure);
    if (wallpaper == NULL)
    {
        return EXIT_FAILURE;
    }

    // Wait for wallpaper to be drawn before spawning processes.
    msg_t msg = {0};
    while (msg.type != LMSG_QUIT && msg.type != LMSG_REDRAW)
    {
        win_receive(wallpaper, &msg, NEVER);
        procedure(wallpaper, &msg);
    }

    spawn("/bin/cursor.elf");
    spawn("/bin/taskbar.elf");
    spawn("/bin/calculator.elf");

    while (msg.type != LMSG_QUIT)
    {
        win_receive(wallpaper, &msg, NEVER);
        procedure(wallpaper, &msg);
    }

    win_free(wallpaper);
    return EXIT_SUCCESS;
}
