#include <stdio.h>
#include <sys/io.h>

#include "user/common/syscalls.h"

tid_t gettid(void)
{
    return _syscall_gettid();
}
