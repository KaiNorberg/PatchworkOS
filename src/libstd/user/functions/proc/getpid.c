#include <stdio.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

pid_t getpid(void)
{
    return _syscall_getpid();
}
