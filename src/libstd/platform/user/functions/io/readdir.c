#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t readdir(fd_t fd, stat_t* infos, uint64_t amount)
{
    uint64_t result = _SyscallReaddir(fd, infos, amount);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
