#include <stdio.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

pid_t getpid(void)
{
    pid_t result = _syscall_getpid();
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
