#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "user/common/syscalls.h"

size_t write(fd_t fd, const void* buffer, size_t count)
{
    uint64_t result = _syscall_write(fd, buffer, count);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
