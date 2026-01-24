#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

fd_t openat(fd_t from, const char* path)
{
    fd_t fd = _syscall_openat(from, path);
    if (fd == ERR)
    {
        errno = _syscall_errno();
    }
    return fd;
}
