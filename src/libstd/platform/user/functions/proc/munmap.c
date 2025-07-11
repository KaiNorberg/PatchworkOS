#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t munmap(void* address, uint64_t length)
{
    uint64_t result = _syscall_munmap(address, length);
    if (result == ERR)
    {
        errno = _syscall_last_error();
    }
    return result;
}
