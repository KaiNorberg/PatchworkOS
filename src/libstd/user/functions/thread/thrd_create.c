#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/proc.h>
#include <threads.h>

#include "user/common/syscalls.h"
#include "user/common/thread.h"

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg)
{
    _thread_t* thread = _thread_new(func, arg);
    if (thread == NULL)
    {
        return thrd_error;
    }

    thr->id = thread->id;
    return thrd_success;
}
