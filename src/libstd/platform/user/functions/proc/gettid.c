#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

tid_t gettid(void)
{
    tid_t result = _SyscallGettid();
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
