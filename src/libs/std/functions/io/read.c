#include <sys/io.h>

#include "libs/std/internal/syscalls.h"

uint64_t read(fd_t fd, void* buffer, uint64_t count)
{
    uint64_t result = SYSCALL(SYS_READ, 3, fd, buffer, count);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}