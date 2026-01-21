#include <stdio.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

void* munmap(void* address, size_t length)
{
    void* result = _syscall_munmap(address, length);
    if (result == NULL)
    {
        errno = _syscall_errno();
    }
    return result;
}
