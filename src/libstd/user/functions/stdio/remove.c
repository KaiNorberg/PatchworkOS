#include <stdio.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

int remove(const char* pathname)
{
    uint64_t result = _syscall_remove(pathname);
    if (result == ERR)
    {
        errno = _syscall_errno();
        return EOF;
    }

    return 0;
}
