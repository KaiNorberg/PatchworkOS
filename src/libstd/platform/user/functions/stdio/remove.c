#include <stdio.h>

#include "platform/user/common/syscalls.h"

int remove(const char* pathname)
{
    if (_syscall_remove(pathname) == ERR)
    {
        errno = _syscall_last_error();
        return EOF;
    }
    return 0;
}
