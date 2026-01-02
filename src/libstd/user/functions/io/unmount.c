#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t unmount(const char* mountpoint)
{
    if (_syscall_umount(mountpoint) == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }

    return 0;
}