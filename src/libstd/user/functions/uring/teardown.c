#include <sys/uring.h>

#include "user/common/syscalls.h"

uint64_t teardown(ring_id_t id)
{
    uint64_t result = _syscall_teardown(id);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}