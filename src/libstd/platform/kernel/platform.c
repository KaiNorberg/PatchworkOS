#include "../platform.h"

#include "log/log.h"
#include "log/panic.h"
#include "sched/thread.h"

#include <sys/math.h>
#include <sys/proc.h>

void _platform_early_init(void)
{
    LOG_DEBUG("kernel stdlib early init\n");
}

void _platform_late_init(void)
{
    LOG_DEBUG("kernel stdlib late init\n");
}

int* _platform_errno_get(void)
{
    thread_t* thread = sched_thread();
    if (thread == NULL)
    {
        static int garbage;
        return &garbage;
    }

    return &thread->error;
}

void _platform_abort(const char* message)
{
    if (message != NULL)
    {
        panic(NULL, message);
    }
    else
    {
        panic(NULL, "libstd unknown abort");
    }
}
