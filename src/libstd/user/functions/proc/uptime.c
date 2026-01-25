#include <stdio.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

clock_t uptime(void)
{
    clock_t result = _syscall_uptime();
    if (result == _FAIL)
    {
        errno = _syscall_errno();
    }
    return result;
}
