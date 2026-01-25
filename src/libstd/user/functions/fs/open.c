#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

fd_t open(const char* path)
{
    fd_t fd = _syscall_open(path);
    if (fd == _FAIL)
    {
        errno = _syscall_errno();
    }
    return fd;
}
