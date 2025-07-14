#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t open2(const char* path, fd_t fds[2])
{
    uint64_t result = _syscall_open2(path, fds);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
