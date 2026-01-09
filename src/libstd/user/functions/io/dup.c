#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

fd_t dup(fd_t oldFd)
{
    uint64_t result = _syscall_dup(oldFd);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
