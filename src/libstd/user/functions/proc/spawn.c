#include <stdio.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

pid_t spawn(const char** argv, spawn_flags_t flags)
{
    pid_t result = _syscall_spawn(argv, flags);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
