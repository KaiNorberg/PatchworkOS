#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

static win_theme_t theme;

static uint64_t procedure(win_t* window, surface_t* surface, msg_t type, void* data)
{
    switch (type)
    {
    case LMSG_INIT:
    {
    }
    break;
    case LMSG_REDRAW:
    {
        rect_t rect = RECT_INIT_SURFACE(surface);
        gfx_rect(surface, &rect, 0xFF007E81);
    }
    break;
    case LMSG_QUIT:
    {
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

    win_t* wallpaper = win_new("Wallpaper", &rect, &theme, procedure, WIN_WALL);
    if (wallpaper == NULL)
    {
        exit(EXIT_FAILURE);
    }

    // Wait for wallpaper to be drawn before spawning processes.
    while (win_receive(wallpaper, NEVER) != LMSG_REDRAW)
    {
    }

    spawn("/bin/cursor.elf");
    spawn("/bin/taskbar.elf");

    spawn("/bin/calculator.elf");
    spawn("/bin/calculator.elf");
    spawn("/bin/calculator.elf");
    spawn("/bin/calculator.elf");

    while (win_receive(wallpaper, NEVER) != LMSG_QUIT)
    {
    }

    win_free(wallpaper);

    return EXIT_SUCCESS;
}
