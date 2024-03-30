#include <sys/process.h>

#include "internal/syscalls/syscalls.h"

pid_t getpid(void)
{
    return SYSCALL(SYS_PID, 0);
}