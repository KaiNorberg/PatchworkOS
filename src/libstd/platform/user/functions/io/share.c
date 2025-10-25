#include <errno.h>
#include <sys/io.h>

#include "platform/user/common/syscalls.h"

uint64_t share(key_t* key, fd_t fd, clock_t timeout)
{
    if (_syscall_share(key, fd, timeout) == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }
    return 0;
}
