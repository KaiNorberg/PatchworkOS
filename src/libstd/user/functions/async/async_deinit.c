#include <sys/async.h>

#include "user/common/syscalls.h"

uint64_t async_deinit(void)
{
    uint64_t result = _syscall_async_deinit();
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}