#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

clock_t uptime(void)
{
    clock_t result = _syscall_uptime();
    if (result == ERR)
    {
        errno = _syscall_last_error();
    }
    return result;
}
