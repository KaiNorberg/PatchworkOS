#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

uint64_t close(fd_t fd)
{
    uint64_t result = _syscall_close(fd);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
