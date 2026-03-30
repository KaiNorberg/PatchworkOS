#include <libc/proc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads.h>

#include "user/common/threading.h"

void thrd_yield(void)
{
    syscall0(SYS_THRD_YIELD, NULL);
}
