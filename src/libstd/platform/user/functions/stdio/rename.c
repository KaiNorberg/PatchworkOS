#include <stdio.h>

#include "platform/user/common/syscalls.h"

int rename(const char* oldpath, const char* newpath)
{
    if (_syscall_rename(oldpath, newpath) == ERR)
    {
        errno = _syscall_last_error();
        return EOF;
    }
    return 0;
}
