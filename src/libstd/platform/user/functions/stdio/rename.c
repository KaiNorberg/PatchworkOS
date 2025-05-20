#include <stdio.h>

#include "platform/user/common/syscalls.h"

int rename(const char* oldpath, const char* newpath)
{
    if (_SyscallRename(oldpath, newpath) == ERR)
    {
        errno = _SyscallLastError();
        return EOF;
    }
    return 0;
}
