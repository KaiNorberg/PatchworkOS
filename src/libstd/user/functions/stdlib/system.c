#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/proc.h>

int system(const char* command)
{
    const char* argv[] = {"/bin/shell", command, NULL};
    pid_t shell = spawn(argv, SPAWN_DEFAULT);
    if (shell == _FAIL)
    {
        return -1;
    }

    fd_t wait = open(F("/proc/%d/wait", shell));
    if (wait == _FAIL)
    {
        return -1;
    }

    char buf[MAX_PATH];
    if (read(wait, buf, MAX_PATH) == _FAIL)
    {
        close(wait);
        return -1;
    }

    close(wait);
    return atoi(buf);
}
