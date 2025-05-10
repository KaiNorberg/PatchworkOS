#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

thrd_t thrd_current(void)
{
    _Thread_t* thread = _ThreadById(_SyscallThreadId());
    thrd_t thr = {.thread = thread};

    _ThreadUnref(thread);
    return thr;
}
