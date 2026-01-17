#include <sys/rings.h>

#include "user/common/syscalls.h"

uint64_t teardown(void)
{
    uint64_t result = _syscall_teardown();
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}