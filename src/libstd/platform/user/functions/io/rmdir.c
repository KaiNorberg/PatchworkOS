#include <sys/io.h>

#include "platform/user/common/syscalls.h"

uint64_t rmdir(const char* path)
{
    uint64_t result = _syscall_rmdir(path);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }

    return result;
}
