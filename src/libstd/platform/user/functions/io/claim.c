#include <sys/io.h>
#include <errno.h>

#include "platform/user/common/syscalls.h"

fd_t claim(key_t* key)
{
    fd_t fd = _syscall_claim(key);
    if (fd == ERR)
    {
        errno = _syscall_errno();
        return ERR;
    }
    return fd;
}
