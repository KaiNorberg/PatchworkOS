#include <sys/rings.h>

#include "user/common/syscalls.h"

uint64_t enter(size_t amount, size_t wait)
{
    uint64_t result = _syscall_enter(amount, wait);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}