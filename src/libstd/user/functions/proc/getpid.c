#include <stdio.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

pid_t getpid(void)
{
    return _syscall_getpid();
}
