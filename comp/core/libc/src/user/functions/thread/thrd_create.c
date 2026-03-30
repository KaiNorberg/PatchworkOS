#include <libc/proc.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "user/common/threading.h"

int thrd_create(thrd_t* thr, thrd_start_t func, void* arg)
{
    _thread_t* thread = _thread_new(func, arg);
    if (thread == NULL)
    {
        return thrd_error;
    }

    *thr = thread->id;
    return thrd_success;
}
