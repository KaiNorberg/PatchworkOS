#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t mount(const char* device, const char* fs, const char* mountpoint)
{
    if (_syscall_mount(device, fs, mountpoint) == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }

    return 0;
}