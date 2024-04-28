#include <sys/ioctl.h>

#include "libs/std/internal/syscalls.h"

uint64_t ioctl(fd_t fd, uint64_t request, void* buffer, uint64_t length)
{
    uint64_t result = SYSCALL(SYS_IOCTL, 4, fd, request, buffer, length);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}