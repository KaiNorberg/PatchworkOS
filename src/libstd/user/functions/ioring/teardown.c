#include <sys/ioring.h>

#include "user/common/syscalls.h"

uint64_t teardown(io_id_t id)
{
    uint64_t result = _syscall_teardown(id);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}