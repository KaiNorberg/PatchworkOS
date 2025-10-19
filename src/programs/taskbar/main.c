#include "taskbar.h"

#include <libpatchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    display_t* disp = display_new();
    if (disp == NULL)
    {
        printf("Failed to create display\n");
        return EXIT_FAILURE;
    }

    taskbar_t taskbar;
    taskbar_init(&taskbar, disp);

    event_t event = {0};
    while (display_is_connected(disp))
    {
        display_next_event(disp, &event, CLOCKS_NEVER);
        display_dispatch(disp, &event);
    }

    taskbar_deinit(&taskbar);

    display_free(disp);
    return 0;
}
