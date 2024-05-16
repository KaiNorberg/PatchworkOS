#include <sys/io.h>

#include "libs/std/internal/syscalls.h"

uint64_t chdir(const char* path)
{
    uint64_t result = SYSCALL(SYS_CHDIR, 1, path);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}