#include "start_menu.h"

#include <sys/proc.h>
#include <sys/io.h>

static pid_t pid = ERR;

void start_menu_open(void)
{
    const char* argv[] = {"home:/bin/start_menu", NULL};
    pid = spawn(argv, NULL);
}

void start_menu_close(void)
{
    fd_t procFile = procfd(pid);
    if (procFile == ERR)
    {
        return;
    }

    ioctl(procFile, IOCTL_PROC_KILL, NULL, 0);

    close(procFile);
}

bool start_menu_is_open(void)
{
    return pid != ERR;
}