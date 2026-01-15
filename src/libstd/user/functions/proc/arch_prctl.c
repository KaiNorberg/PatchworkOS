#include <stdio.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

uint64_t arch_prctl(arch_prctl_t op, uintptr_t addr)
{
    uint64_t result = _syscall_arch_prctl(op, addr);
    if (result == ERR)
    {
        errno = _syscall_errno();
    }
    return result;
}
