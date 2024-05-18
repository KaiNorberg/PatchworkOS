#include <sys/io.h>

#include "libs/std/internal/syscalls.h"

uint64_t poll(pollfd_t* fds, uint64_t amount, uint64_t timeout)
{
    uint64_t result = SYSCALL(SYS_POLL, 3, fds, amount, timeout);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}