#include <sys/process.h>

#include "internal/syscalls/syscalls.h"

pid_t spawn(const char* path)
{
    pid_t result = SYSCALL(SYS_SPAWN, 1, path);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}