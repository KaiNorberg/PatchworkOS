#include <stdlib.h>

#include "internal/syscalls/syscalls.h"

_NORETURN void exit(int status)
{
    SYSCALL(SYS_PROCESS_EXIT, 1, status);
    while (1)
    {
        asm volatile("ud2");
    }
}