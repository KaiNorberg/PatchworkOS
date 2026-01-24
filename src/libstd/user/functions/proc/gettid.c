#include <stdio.h>
#include <sys/fs.h>

#include "user/common/syscalls.h"

tid_t gettid(void)
{
    return _syscall_gettid();
}
