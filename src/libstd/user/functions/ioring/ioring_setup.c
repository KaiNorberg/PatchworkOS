#include <sys/ioring.h>

#include "user/common/syscalls.h"

ioring_id_t ioring_setup(ioring_t* ring, void* address, size_t sentries, size_t centries)
{
    ioring_id_t result = _syscall_ioring_setup(ring, address, sentries, centries);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}