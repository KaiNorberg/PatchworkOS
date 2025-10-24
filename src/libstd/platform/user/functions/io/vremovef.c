#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/io.h>

#include "platform/user/common/syscalls.h"

uint64_t vremovef(const char* format, va_list args)
{
    char path[MAX_PATH];
    int count = vsnprintf(path, sizeof(path), format, args);
    if (count < 0 || (uint64_t)count >= sizeof(path))
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t result = _syscall_remove(path);
    if (result == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }

    return 0;
}
