#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

uint64_t poll(pollfd_t* fds, uint64_t amount, clock_t timeout)
{
    uint64_t result = _syscall_poll(fds, amount, timeout);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
