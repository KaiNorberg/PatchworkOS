#include "taskbar.h"

#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    display_t* disp = display_new();
    if (disp == NULL)
    {
        printf("taskbar: failed to create display\n");
        return EXIT_FAILURE;
    }

    window_t* win = taskbar_new(disp);
    if (win == NULL)
    {
        printf("taskbar: failed to create taskbar\n");
        display_free(disp);
        return EXIT_FAILURE;
    }

    event_t event = {0};
    while (display_next_event(disp, &event, CLOCKS_NEVER) != ERR)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);
    return 0;
}
