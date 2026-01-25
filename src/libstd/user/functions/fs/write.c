#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

size_t write(fd_t fd, const void* buffer, size_t count)
{
    uint64_t result = _syscall_write(fd, buffer, count);
    if (result == _FAIL)
    {
        errno = _syscall_errno();
    }
    return result;
}
