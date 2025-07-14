#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

fd_t open(const char* path)
{
    fd_t fd = _syscall_open(path);
    if (fd == ERR)
    {
        errno = _syscall_errno();
    }
    return fd;
}
