#include <stdlib.h>

#include "libs/std/internal/syscalls.h"

_NORETURN void exit(int status)
{
    SYSCALL(SYS_PROCESS_EXIT, 1, (uint64_t)status);
    while (1)
    {
        asm volatile("ud2");
    }
}