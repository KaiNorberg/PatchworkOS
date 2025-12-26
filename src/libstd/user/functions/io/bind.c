#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t bind(fd_t source, const char* mountpoint)
{
    if (_syscall_bind(source, mountpoint) == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }

    return 0;
}
