#include <sys/io.h>

#include "libs/std/internal/syscalls.h"

uint64_t seek(fd_t fd, int64_t offset, uint8_t origin)
{
    uint64_t result = SYSCALL(SYS_SEEK, 3, fd, offset, origin);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}