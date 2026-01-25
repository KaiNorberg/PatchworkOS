#include <sys/fs.h>

#include "user/common/syscalls.h"

uint64_t mount(const char* mountpoint, const char* fs, const char* options)
{
    if (_syscall_mount(mountpoint, fs, options) == _FAIL)
    {
        errno = _syscall_errno();
        return _FAIL;
    }

    return 0;
}