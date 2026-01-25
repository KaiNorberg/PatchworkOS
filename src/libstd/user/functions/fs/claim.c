#include <errno.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

fd_t claim(const char* key)
{
    fd_t fd = _syscall_claim(key);
    if (fd == _FAIL)
    {
        errno = _syscall_errno();
        return _FAIL;
    }
    return fd;
}
