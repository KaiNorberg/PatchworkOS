#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

#include "common/print.h"
#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t seek(fd_t fd, int64_t offset, seek_origin_t origin)
{
    uint64_t result = _syscall_seek(fd, offset, origin);
    if (result == ERR)
    {
        errno = _syscall_last_error();
    }
    return result;
}
