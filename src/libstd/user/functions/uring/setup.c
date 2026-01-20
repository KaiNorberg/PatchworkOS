#include <sys/uring.h>

#include "user/common/syscalls.h"

ring_id_t setup(ring_t* ring, void* address, size_t sentries, size_t centries)
{
    ring_id_t result = _syscall_setup(ring, address, sentries, centries);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}