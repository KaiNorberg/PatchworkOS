#include <stdio.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

void* mmap(fd_t fd, void* address, size_t length, prot_t prot)
{
    void* result = _syscall_mmap(fd, address, length, prot);
    if (result == NULL)
    {
        errno = _syscall_errno();
    }
    return result;
}
