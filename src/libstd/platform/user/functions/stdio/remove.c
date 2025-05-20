#include <stdio.h>

#include "platform/user/common/syscalls.h"

int remove(const char* pathname)
{
    if (_SyscallRemove(pathname) == ERR)
    {
        errno = _SyscallLastError();
        return EOF;
    }
    return 0;
}
