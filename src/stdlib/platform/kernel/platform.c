#include "../platform.h"
#include "../../common/print.h"

#include "log.h"
#include "pmm.h"
#include "sched.h"
#include "systime.h"
#include "vmm.h"

#include <stdio.h>
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

int _PlatformVprintf(const char* _RESTRICT format, va_list args)
{
    char buffer[MAX_PATH];

    nsec_t time = log_time_enabled() ? systime_uptime() : 0;
    nsec_t sec = time / SEC;
    nsec_t ms = (time % SEC) / (SEC / 1000);

    uint64_t result = vsprintf(buffer + sprintf(buffer, "[%10llu.%03llu] ", sec, ms), format, args);

    char newline[] = {'\n', '\0'};
    strcat(buffer, newline);
    log_write(buffer);
    return result + 1;
}