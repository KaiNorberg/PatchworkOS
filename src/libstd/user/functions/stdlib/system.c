#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/proc.h>

int system(const char* command)
{
    const char* argv[] = {"/bin/shell", command, NULL};
    pid_t shell;
    status_t status = spawn(argv, SPAWN_DEFAULT, &shell);
    if (IS_ERR(status))
    {
        return -1;
    }

    fd_t wait;
    status = open(&wait, F("/proc/%d/wait", shell));
    if (IS_ERR(status))
    {
        return -1;
    }

    char buf[MAX_PATH];
    status = read(wait, buf, MAX_PATH, NULL);
    if (IS_ERR(status))
    {
        close(wait);
        return -1;
    }

    close(wait);
    return atoi(buf);
}
