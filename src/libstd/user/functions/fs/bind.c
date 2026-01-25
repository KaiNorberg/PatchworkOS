#include <sys/fs.h>

#include "user/common/syscalls.h"

uint64_t bind(const char* mountpoint, fd_t source)
{
    if (_syscall_bind(mountpoint, source) == _FAIL)
    {
        errno = _syscall_errno();
        return _FAIL;
    }

    return 0;
}
