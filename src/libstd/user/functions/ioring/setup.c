#include <sys/ioring.h>

#include "user/common/syscalls.h"

io_id_t setup(ioring_t* ring, void* address, size_t sentries, size_t centries)
{
    io_id_t result = _syscall_setup(ring, address, sentries, centries);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}