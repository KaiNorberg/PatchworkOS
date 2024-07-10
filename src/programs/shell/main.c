#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

static win_theme_t theme;

static uint64_t procedure(win_t* window, surface_t* surface, const msg_t* msg)
{
    switch (msg->type)
    {
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
    win_theme_t theme;
    win_default_theme(&theme);

    rect_t rect;
    win_screen_rect(&rect);

    win_t* wallpaper = win_new("Wallpaper", &rect, &theme, procedure, WIN_WALL);
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
