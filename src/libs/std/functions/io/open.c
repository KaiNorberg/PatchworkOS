#include <sys/io.h>

#include "libs/std/internal/syscalls.h"

fd_t open(const char* path)
{    
    fd_t result = SYSCALL(SYS_OPEN, 1, path);
    if (result == ERR)
    {
        errno = SYSCALL(SYS_ERROR, 0);
    }
    return result;
}