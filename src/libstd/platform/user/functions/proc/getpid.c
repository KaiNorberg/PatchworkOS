#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

pid_t getpid(void)
{
    pid_t result = _syscall_getpid();
    if (result == ERR)
    {
        errno = _syscall_last_error();
    }
    return result;
}
