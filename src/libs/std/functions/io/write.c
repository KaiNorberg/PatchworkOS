#include <sys/io.h>

#include "libs/std/internal/syscalls.h"

uint64_t write(fd_t fd, const void* buffer, uint64_t count)
{
    uint64_t result = SYSCALL(SYS_WRITE, 3, fd, buffer, count);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}