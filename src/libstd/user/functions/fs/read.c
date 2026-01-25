#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

size_t read(fd_t fd, void* buffer, size_t count)
{
    uint64_t result = _syscall_read(fd, buffer, count);
    if (result == _FAIL)
    {
        errno = _syscall_errno();
    }
    return result;
}
