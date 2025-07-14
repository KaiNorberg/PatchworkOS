#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

tid_t gettid(void)
{
    tid_t result = _syscall_gettid();
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
