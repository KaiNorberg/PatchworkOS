#include "taskbar.h"

#include <patchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    fd_t klog = open("/dev/klog");
    if (klog == _FAIL)
    {
        printf("taskbar: failed to open klog\n");
        return EXIT_FAILURE;
    }
    if (dup2(klog, STDOUT_FILENO) == _FAIL || dup2(klog, STDERR_FILENO) == _FAIL)
    {
        printf("taskbar: failed to redirect stdout/stderr to klog\n");
        close(klog);
        return EXIT_FAILURE;
    }
    close(klog);

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
    while (display_next(disp, &event, CLOCKS_NEVER) != _FAIL)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);
    return 0;
}
