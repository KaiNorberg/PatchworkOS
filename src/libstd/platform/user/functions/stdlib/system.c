#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>

int system(const char* command)
{
    spawn_fd_t fds[] = {
        {.child = STDIN_FILENO, .parent = STDIN_FILENO},
        {.child = STDOUT_FILENO, .parent = STDOUT_FILENO},
        {.child = STDERR_FILENO, .parent = STDERR_FILENO},
        SPAWN_FD_END,
    };
    const char* argv[] = {"/bin/shell", command, NULL};
    pid_t shell = spawn(argv, fds, NULL, NULL);
    if (shell == ERR)
    {
        return -1;
    }

    fd_t status = openf("/proc/%d/status", shell);
    if (status == ERR)
    {
        return -1;
    }

    char buf[MAX_PATH];
    if (read(status, buf, MAX_PATH) == ERR)
    {
        close(status);
        return -1;
    }

    close(status);
    return atoi(buf);
}
