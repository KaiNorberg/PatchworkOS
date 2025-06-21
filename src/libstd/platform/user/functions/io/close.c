#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t close(fd_t fd)
{
    uint64_t result = _SyscallClose(fd);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
