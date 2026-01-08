#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "user/common/syscalls.h"

size_t read(fd_t fd, void* buffer, size_t count)
{
    uint64_t result = _syscall_read(fd, buffer, count);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
