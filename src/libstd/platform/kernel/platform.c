#include "../platform.h"
#include "common/print.h"

#include "drivers/systime/systime.h"
#include "log/log.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "sched/thread.h"

#include <stdio.h>
#include <sys/math.h>
#include <sys/proc.h>

void _platform_early_init(void)
{
}

void _platform_late_init(void)
{
}

int* _platform_errno_get(void)
{
    return &sched_thread()->error;
}

void _platform_abort(const char* message)
{
    if (message != NULL)
    {
        log_panic(NULL, message);
    }
    else
    {
        log_panic(NULL, "libstd unknown abort");
    }
}
