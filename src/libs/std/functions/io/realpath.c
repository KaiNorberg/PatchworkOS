#include <sys/io.h>

#include "libs/std/internal/syscalls.h"

uint64_t realpath(char* out, const char* path)
{
    uint64_t result = SYSCALL(SYS_REALPATH, 2, out, path);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}