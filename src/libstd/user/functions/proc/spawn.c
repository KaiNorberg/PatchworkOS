#include <stdio.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

pid_t spawn(const char** argv, const spawn_fd_t* fds, const char* cwd, priority_t priority, spawn_flags_t flags)
{
    pid_t result = _syscall_spawn(argv, fds, cwd, priority, flags);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
