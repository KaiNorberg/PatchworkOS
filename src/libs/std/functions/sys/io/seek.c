#include <sys/io.h>

#include "internal/syscalls/syscalls.h"

uint64_t seek(fd_t fd, int64_t offset, uint8_t origin)
{
    uint64_t result = SYSCALL(SYS_SEEK, 3, fd, offset, origin);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_KERNEL_ERRNO, 0);
    }
    return result;
}