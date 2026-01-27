#include <stdio.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

int remove(const char* pathname)
{
    status_t status = syscall1(SYS_REMOVE, NULL, (uintptr_t)pathname);
    if (IS_ERR(status))
    {
        return EOF;
    }

    return 0;
}
