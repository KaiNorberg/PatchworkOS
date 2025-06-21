#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

fd_t dup2(fd_t oldFd, fd_t newFd)
{
    uint64_t result = _SyscallDup2(oldFd, newFd);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
