#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

void thrd_yield(void)
{
    _SyscallYield();
}
