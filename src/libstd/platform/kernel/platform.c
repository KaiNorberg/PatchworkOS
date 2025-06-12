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

static uintptr_t newAddress;
extern uintptr_t _kernelEnd;

static lock_t allocLock;

void _PlatformEarlyInit(void)
{
    newAddress = ROUND_UP((uint64_t)&_kernelEnd, PAGE_SIZE);
    lock_init(&allocLock);
}

void _PlatformLateInit(void)
{
}

void* _PlatformPageAlloc(uint64_t amount)
{
    if (amount == 0)
    {
        return ERRPTR(EINVAL);
    }

    LOCK_DEFER(&allocLock);
    void* startAddr = (void*)newAddress;

    for (uint64_t i = 0; i < amount; i++)
    {
        void* addr = (void*)((uint64_t)startAddr + i * PAGE_SIZE);
        void* page = pmm_alloc();

        if (page == NULL || vmm_kernel_map(addr, page, 1, PML_WRITE | PML_OWNED) == NULL)
        {
            if (page != NULL)
            {
                pmm_free(page);
            }

            // Page table will free the previously allocated pages as they are owned by the Page table.
            vmm_kernel_unmap(addr, i);
            return ERRPTR(ENOMEM);
        }
    }

    newAddress += amount * PAGE_SIZE;
    return startAddr;
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
