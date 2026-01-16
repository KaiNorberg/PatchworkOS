#include <sys/async.h>

#include "user/common/syscalls.h"

uint64_t async_init(async_rings_t* rings, void* address, size_t sentries, size_t centries)
{
    uint64_t result = _syscall_async_init(rings, address, sentries, centries);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}