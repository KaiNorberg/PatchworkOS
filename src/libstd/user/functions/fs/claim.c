#include <errno.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

fd_t claim(const char* key)
{
    fd_t fd = _syscall_claim(key);
    if (fd == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }
    return fd;
}
