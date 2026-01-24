#include <sys/ioring.h>

#include "user/common/syscalls.h"

uint64_t ioring_enter(ioring_id_t id, size_t amount, size_t wait)
{
    uint64_t result = _syscall_ioring_enter(id, amount, wait);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}