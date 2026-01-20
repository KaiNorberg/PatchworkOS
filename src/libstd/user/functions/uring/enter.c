#include <sys/uring.h>

#include "user/common/syscalls.h"

uint64_t enter(ring_id_t id, size_t amount, size_t wait)
{
    uint64_t result = _syscall_enter(id, amount, wait);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}