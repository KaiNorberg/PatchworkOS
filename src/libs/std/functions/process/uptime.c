#include <sys/process.h>

#include "libs/std/internal/syscalls.h"

_PUBLIC uint64_t uptime(void)
{
    uint64_t result = SYSCALL(SYS_UPTIME, 0);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
        return ERR;
    }
    return result;
}