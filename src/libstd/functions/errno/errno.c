#include <errno.h>

#ifdef _KERNEL_
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#else
#include "user/common/thread.h"
#include <sys/proc.h>
#endif

int* _errno_get(void)
{
    static int garbage;

#ifdef _KERNEL_
    thread_t* thread = sched_thread();
    if (thread == NULL)
    {
        return &garbage;
    }

    return &thread->error;
#else
    _thread_t* thread = _thread_get(gettid());
    if (thread == NULL)
    {
        return &garbage;
    }
    return &thread->err;
#endif
}
