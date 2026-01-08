#include <stdio.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

void* mprotect(void* address, size_t length, prot_t prot)
{
    void* result = _syscall_mprotect(address, length, prot);
    if (result == NULL)
    {
        errno = _syscall_errno();
    }
    return result;
}
