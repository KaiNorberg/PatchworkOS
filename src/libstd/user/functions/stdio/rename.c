#include <stdio.h>

#include "user/common/syscalls.h"

int rename(const char* oldpath, const char* newpath)
{
    if (_syscall_link(oldpath, newpath) == _FAIL)
    {
        errno = _syscall_errno();
        return EOF;
    }
    if (_syscall_remove(oldpath) == _FAIL)
    {
        errno = _syscall_errno();
        return EOF;
    }
    return 0;
}
