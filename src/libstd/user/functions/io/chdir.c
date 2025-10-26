#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "user/common/syscalls.h"

uint64_t chdir(const char* path)
{
    uint64_t result = _syscall_chdir(path);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
