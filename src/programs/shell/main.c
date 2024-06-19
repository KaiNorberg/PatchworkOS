#include "taskbar.h"

#include <stdlib.h>
#include <sys/gfx.h>
#include <sys/kbd.h>
#include <sys/proc.h>

int main(void)
{
    win_t* taskbar = taskbar_init();

    while (win_dispatch(taskbar, NEVER) != LMSG_QUIT)
    {
    }

    win_free(taskbar);

    return EXIT_SUCCESS;
}
