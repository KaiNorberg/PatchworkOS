#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

uint64_t stat(const char* path, stat_t* info)
{
    uint64_t result = _syscall_stat(path, info);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
