#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t read(fd_t fd, void* buffer, uint64_t count)
{
    uint64_t result = _syscall_read(fd, buffer, count);
    if (result == ERR)
    {
        errno = _syscall_last_error();
    }
    return result;
}
