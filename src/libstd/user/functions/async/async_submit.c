#include <sys/async.h>

#include "user/common/syscalls.h"

uint64_t async_notify(size_t amount, size_t wait)
{
    uint64_t result = _syscall_async_notify(amount, wait);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}