#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t poll(pollfd_t* fds, uint64_t amount, clock_t timeout)
{
    uint64_t result = _SyscallPoll(fds, amount, timeout);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
