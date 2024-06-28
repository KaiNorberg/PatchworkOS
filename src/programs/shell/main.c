#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/proc.h>
#include <sys/win.h>

#include "wallpaper.h"

int main(void)
{
    win_t* wallpaper = wallpaper_init();

    // Wait for wallpaper to be drawn before spawning processes.
    while (win_dispatch(wallpaper, NEVER) != LMSG_REDRAW)
    {
    }

    spawn("home:/bin/cursor");
    spawn("home:/bin/taskbar");

    spawn("home:/bin/calculator");
    spawn("home:/bin/calculator");
    spawn("home:/bin/calculator");
    spawn("home:/bin/calculator");

    while (win_dispatch(wallpaper, NEVER) != LMSG_QUIT)
    {
    }

    win_free(wallpaper);

    return EXIT_SUCCESS;
}
