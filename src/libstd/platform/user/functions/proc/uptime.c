#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

clock_t uptime(void)
{
    clock_t result = _SyscallUptime();
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
