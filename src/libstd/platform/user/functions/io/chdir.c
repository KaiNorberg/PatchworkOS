#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t chdir(const char* path)
{
    uint64_t result = _SyscallChdir(path);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
