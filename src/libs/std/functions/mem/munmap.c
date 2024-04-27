#include <sys/mem.h>

#include "libs/std/internal/syscalls.h"

uint64_t munmap(void* address, uint64_t length)
{
    uint64_t result = SYSCALL(SYS_MUNMAP, 2, address, length);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}