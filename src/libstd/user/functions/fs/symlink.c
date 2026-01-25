#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

uint64_t symlink(const char* target, const char* linkpath)
{
    uint64_t result = _syscall_symlink(target, linkpath);
    if (result == _FAIL)
    {
        errno = _syscall_errno();
    }
    return result;
}