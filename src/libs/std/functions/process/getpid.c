#include <sys/process.h>

#include "libs/std/internal/syscalls.h"

pid_t getpid(void)
{
    return SYSCALL(SYS_PID, 0);
}