#include <stdio.h>

#include "platform/user/common/syscalls.h"

int rename(const char* oldpath, const char* newpath)
{
    if (_syscall_link(oldpath, newpath) == ERR)
    {
        errno = _syscall_errno();
        return EOF;
    }
    if (_syscall_remove(oldpath) == ERR)
    {
        errno = _syscall_errno();
        return EOF;
    }
    return 0;
}
