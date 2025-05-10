#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/proc.h>
#include <threads.h>

#include "platform/user/common/syscalls.h"
#include "platform/user/common/thread.h"

int thrd_join(thrd_t thr, int* res)
{
    _Thread_t* thread = _ThreadRef(thr.thread);
    futex(&thread->running, true, FUTEX_WAIT, CLOCKS_NEVER);
    if (res != NULL)
    {
        *res = thread->result;
    }
    _ThreadUnref(thread);
    return thrd_success;
}
