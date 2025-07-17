#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/proc.h>

int system(const char* command)
{
    // TODO: Implement process status return.

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

    fd_t ctl = openf("/proc/%d/ctl", shell);
    if (ctl == ERR)
    {
        return -1;
    }
    if (writef(ctl, "wait") == ERR)
    {
        return -1;
    }
    if (close(ctl) == ERR)
    {
        return -1;
    }

    return 0;
}
