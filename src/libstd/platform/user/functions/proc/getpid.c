#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

pid_t getpid(void)
{
    pid_t result = _SyscallGetpid();
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
