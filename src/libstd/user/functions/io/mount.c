#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t mount(const char* mountpoint, const char* fs, const char* options)
{
    if (_syscall_mount(mountpoint, fs, options) == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }

    return 0;
}