#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t bind(fd_t source, const char* mountpoint, mount_flags_t flags)
{
    if (_syscall_bind(source, mountpoint, flags) == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }

    return 0;
}
