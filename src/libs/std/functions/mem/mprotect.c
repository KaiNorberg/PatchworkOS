#include <sys/mem.h>

#include "libs/std/internal/syscalls.h"

uint64_t mprotect(void* address, uint64_t length, uint8_t prot)
{
    uint64_t result = SYSCALL(SYS_MPROTECT, 3, address, length, prot);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}