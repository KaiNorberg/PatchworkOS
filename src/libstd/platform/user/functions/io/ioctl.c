#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    uint64_t result = _syscall_ioctl(fd, request, argp, size);
    if (result == ERR)
    {
        errno = _syscall_last_error();
    }
    return result;
}
