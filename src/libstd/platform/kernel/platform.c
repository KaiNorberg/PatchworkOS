#include "../platform.h"
#include "common/print.h"

#include "drivers/systime/systime.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "sched/sched.h"
#include "utils/log.h"

#include <stdio.h>
#include <sys/math.h>
#include <sys/proc.h>

static uintptr_t newAddress;
extern uintptr_t _kernelEnd;

void _PlatformEarlyInit(void)
{
    newAddress = ROUND_UP((uint64_t)&_kernelEnd, PAGE_SIZE);
}

void _PlatformLateInit(void)
{
}

void* _PlatformPageAlloc(uint64_t amount)
{
    void* addr = (void*)newAddress;
    if (vmm_kernel_alloc(addr, amount * PAGE_SIZE) == NULL)
    {
        return NULL;
    }
    newAddress += amount * PAGE_SIZE;

    return addr;
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
