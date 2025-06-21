#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t munmap(void* address, uint64_t length)
{
    uint64_t result = _SyscallMunmap(address, length);
    if (result == ERR)
    {
        errno = _SyscallLastError();
    }
    return result;
}
