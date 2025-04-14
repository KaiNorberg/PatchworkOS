#include "start_menu.h"

#include <sys/io.h>
#include <sys/proc.h>

static pid_t pid = ERR;

void start_menu_open(void)
{
    const char* argv[] = {"home:/bin/startmenu", NULL};
    pid = process_create(argv, NULL);
}

void start_menu_close(void)
{
    fd_t procFile = process_open(pid, "ctl");
    if (procFile == ERR)
    {
        return;
    }

    writef(procFile, "kill");

    close(procFile);
}

bool start_menu_is_open(void)
{
    return pid != ERR;
}
