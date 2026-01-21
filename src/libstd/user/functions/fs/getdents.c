#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

size_t getdents(fd_t fd, dirent_t* buffer, uint64_t count)
{
    uint64_t result = _syscall_getdents(fd, buffer, count);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
