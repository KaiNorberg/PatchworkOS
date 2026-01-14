#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "user/common/syscalls.h"
#include "user/common/threading.h"

void thrd_yield(void)
{
    _syscall_yield();
}
