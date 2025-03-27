#include "../platform.h"

#include "log.h"
#include "pmm.h"
#include "sched.h"
#include "vmm.h"

#include <sched.h>
#include <sys/math.h>
#include <sys/proc.h>

static uintptr_t newAddress;
extern uintptr_t _kernelEnd;

void _PlatformInit(void)
{
    newAddress = ROUND_UP((uint64_t)&_kernelEnd, PAGE_SIZE);
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