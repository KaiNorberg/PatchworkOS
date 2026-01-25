#include <sys/fs.h>

#include "user/common/syscalls.h"

uint64_t unmount(const char* mountpoint)
{
    if (_syscall_umount(mountpoint) == _FAIL)
    {
        errno = _syscall_errno();
        return _FAIL;
    }

    return 0;
}