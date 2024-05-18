#include <sys/process.h>

#include "libs/std/internal/syscalls.h"

_PUBLIC uint64_t sleep(uint64_t nanoseconds)
{
    uint64_t result = SYSCALL(SYS_SLEEP, 1, nanoseconds);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
        return ERR;
    }
    return result;
}