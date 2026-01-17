#include <sys/rings.h>

#include "user/common/syscalls.h"

uint64_t setup(rings_t* rings, void* address, size_t sentries, size_t centries)
{
    uint64_t result = _syscall_setup(rings, address, sentries, centries);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}