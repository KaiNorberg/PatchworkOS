#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sync.h>
#include <sys/syscall.h>
#include <threads.h>

#include "user/common/threading.h"

void thrd_exit(int res)
{
    _thread_t* thread = _THREAD_SELF->self;
    if (thread == NULL)
    {
        proc_exit("libstd: thrd_exit called from unknown thread");
    }

    thread->result = res;

    uint64_t state = atomic_exchange(&thread->state, _THREAD_EXITED);
    if (state == _THREAD_DETACHED)
    {
        _thread_free(thread);
    }
    else
    {
        sync_ctl(&thread->state, UINT64_MAX, SYNC_WAKE, CLOCKS_NEVER, NULL);
    }

    syscall0(SYS_THRD_EXIT, NULL);
    __builtin_unreachable();
}
