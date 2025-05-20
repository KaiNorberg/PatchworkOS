#include "start_menu.h"

#include <stdio.h>
#include <sys/io.h>
#include <sys/proc.h>

static pid_t pid = ERR;

void start_menu_open(void)
{
    const char* argv[] = {"home:/bin/startmenu", NULL};
    pid = spawn(argv, NULL);
}

void start_menu_close(void)
{
    fd_t noteFile = openf("sys:/proc/%d/note", pid);
    if (noteFile == ERR)
    {
        return;
    }

    writef(noteFile, "kill");

    close(noteFile);
}

bool start_menu_is_open(void)
{
    return pid != ERR;
}
