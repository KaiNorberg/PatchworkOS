#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t nanosleep(clock_t timeout)
{
    uint64_t result = _syscall_nanosleep(timeout);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
