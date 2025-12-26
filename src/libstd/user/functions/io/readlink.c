#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "user/common/syscalls.h"

uint64_t readlink(const char* path, char* buffer, uint64_t count)
{
    uint64_t result = _syscall_readlink(path, buffer, count);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}