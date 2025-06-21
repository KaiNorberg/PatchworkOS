#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

pid_t spawn(const char** argv, const spawn_fd_t* fds, const char* cwd, spawn_attr_t* attr)
{
    pid_t result = _SyscallSpawn(argv, fds, cwd, attr);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
