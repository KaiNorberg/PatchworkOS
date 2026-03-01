#include "taskbar.h"

#include <patchwork/patchwork.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    fd_t klog;
    if (IS_ERR(open(&klog, "/dev/klog")))
    {
        printf("taskbar: failed to open klog\n");
        return EXIT_FAILURE;
    }
    fd_t stdoutFd = STDOUT_FILENO;
    fd_t stderrFd = STDERR_FILENO;
    if (IS_ERR(dup(klog, &stdoutFd)) || IS_ERR(dup(klog, &stderrFd)))
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
    while (display_next(disp, &event, CLOCKS_NEVER) != PFAIL)
    {
        display_dispatch(disp, &event);
    }

    window_free(win);
    display_free(disp);
    return 0;
}
