#include <sys/io.h>

#include "internal/syscalls/syscalls.h"

uint64_t close(fd_t fd)
{
    uint64_t result = SYSCALL(SYS_CLOSE, 1, fd);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}