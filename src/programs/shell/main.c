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

        rect_t rect = RECT_INIT_GFX(&gfx);
        gfx_rect(&gfx, &rect, 0xFF007E81);
        // gfx_rect(&gfx, &rect, 0xFF3E77B3);

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
    win_t* wall = win_new("Wallpaper", &rect, DWM_WALL, WIN_NONE, procedure);
    if (wall == NULL)
    {
        return EXIT_FAILURE;
    }

    // Wait for wallpaper to be drawn before spawning processes.
    msg_t msg = {0};
    while (msg.type != LMSG_QUIT && msg.type != LMSG_REDRAW)
    {
        win_receive(wall, &msg, NEVER);
        procedure(wall, &msg);
    }

    spawn("/bin/cursor.elf");
    spawn("/bin/taskbar.elf");
    spawn("/bin/calculator.elf");

    while (msg.type != LMSG_QUIT)
    {
        win_receive(wall, &msg, NEVER);
        procedure(wall, &msg);
    }

    win_free(wall);
    return EXIT_SUCCESS;
}
