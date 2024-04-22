#include <sys/process.h>

#include "libs/std/internal/syscalls.h"

pid_t spawn(const char* path)
{
    uint64_t pid = SYSCALL(SYS_SPAWN, 1, path);
    if (pid == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
        return ERR;
    }
    return pid;
}