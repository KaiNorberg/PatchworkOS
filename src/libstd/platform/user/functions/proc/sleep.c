#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t sleep(clock_t timeout)
{
    uint64_t result = _SyscallSleep(timeout);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}