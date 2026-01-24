#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

size_t seek(fd_t fd, ssize_t offset, seek_origin_t origin)
{
    uint64_t result = _syscall_seek(fd, offset, origin);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
