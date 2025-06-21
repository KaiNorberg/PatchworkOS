#include <stdio.h>
#include <sys/io.h>

#include "platform/platform.h"
#include "platform/user/common/syscalls.h"

uint64_t futex(atomic_uint64_t* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    uint64_t result = _syscall_futex(addr, val, op, timeout);
    if (result == ERR)
    {
        errno = _syscall_last_error();
    }
    return result;
}
