#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

fd_t dup(fd_t oldFd)
{
    uint64_t result = _syscall_dup(oldFd);
    if (result == _FAIL)
    {
        errno = _syscall_errno();
    }
    return result;
}
