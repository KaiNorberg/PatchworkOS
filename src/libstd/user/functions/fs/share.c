#include <errno.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

uint64_t share(char* key, uint64_t size, fd_t fd, clock_t timeout)
{
    if (_syscall_share(key, size, fd, timeout) == _FAIL)
    {
        errno = _syscall_errno();
        return _FAIL;
    }
    return 0;
}
