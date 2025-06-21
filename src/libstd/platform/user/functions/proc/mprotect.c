#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t mprotect(void* address, uint64_t length, prot_t prot)
{
    uint64_t result = _syscall_mprotect(address, length, prot);
    if (result == ERR)
    {
        errno = _syscall_last_error();
    }
    return result;
}
