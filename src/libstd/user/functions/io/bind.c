#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t bind(const char* mountpoint, fd_t source)
{
    if (_syscall_bind(mountpoint, source) == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }

    return 0;
}
