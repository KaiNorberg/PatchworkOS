#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t link(const char* oldPath, const char* newPath)
{
    uint64_t result = _syscall_link(oldPath, newPath);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
