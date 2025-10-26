#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "user/common/syscalls.h"
#include "user/common/thread.h"

thrd_t thrd_current(void)
{
    thrd_t thr = (thrd_t){.id = gettid()};
    return thr;
}
