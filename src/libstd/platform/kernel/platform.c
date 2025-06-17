#include "../platform.h"
#include "common/print.h"

#include "drivers/systime/systime.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "sched/thread.h"
#include "utils/log.h"

#include <stdio.h>
#include <sys/math.h>
#include <sys/proc.h>

void _PlatformEarlyInit(void)
{

}

void _PlatformLateInit(void)
{

}

int* _PlatformErrnoFunc(void)
{
    return &sched_thread()->error;
}

void _PlatformAbort(const char* message)
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
