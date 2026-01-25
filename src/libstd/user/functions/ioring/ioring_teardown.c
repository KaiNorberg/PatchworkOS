#include <sys/ioring.h>

#include "user/common/syscalls.h"

uint64_t ioring_teardown(ioring_id_t id)
{
    uint64_t result = _syscall_ioring_teardown(id);
    if (result == _FAIL)
    {
        errno = _syscall_errno();
    }
    return result;
}