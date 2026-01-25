#include <stdio.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

uint64_t nanosleep(clock_t timeout)
{
    uint64_t result = _syscall_nanosleep(timeout);
    if (result == _FAIL)
    {
        errno = _syscall_errno();
    }
    return result;
}
