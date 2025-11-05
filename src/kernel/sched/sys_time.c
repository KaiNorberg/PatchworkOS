#include <kernel/sched/sys_time.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/smp.h>
#include <kernel/cpu/syscalls.h>
#include <kernel/drivers/apic.h>
#include <kernel/drivers/rtc.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/symbol.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>

#include <kernel/sync/rwlock.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/math.h>

static time_t bootEpoch = 0;
static bool bootEpochInitialized = false;

static sys_time_source_t* sources[SYS_TIME_MAX_SOURCES];
static uint32_t sourceCount = 0;
static sys_time_source_t* bestSource = NULL;
static rwlock_t sourcesLock = RWLOCK_CREATE;

static void timer_boot_epoch_init(void)
{
    struct tm time;
    rtc_read(&time);
    bootEpoch = mktime(&time);
    bootEpochInitialized = true;
}

uint64_t sys_time_register_source(const sys_time_source_t* source)
{
    if (source == NULL || source->read == NULL || source->precision == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    rwlock_write_acquire(&sourcesLock);
    if (sourceCount >= SYS_TIME_MAX_SOURCES)
    {
        rwlock_write_release(&sourcesLock);
        errno = ENOSPC;
        return ERR;
    }

    sources[sourceCount++] = (sys_time_source_t*)source;

    bool bestSourceUpdated = false;
    if (bestSource == NULL || source->precision < bestSource->precision)
    {
        bestSource = (sys_time_source_t*)source;
        bestSourceUpdated = true;
    }
    rwlock_write_release(&sourcesLock);

    LOG_INFO("registered system timer source '%s' with precision %lu ns%s\n", source->name, source->precision,
        bestSourceUpdated ? " (selected as best source)" : "");

    return 0;
}

void sys_time_unregister_source(const sys_time_source_t* source)
{
    if (source == NULL)
    {
        return;
    }

    rwlock_write_acquire(&sourcesLock);
    for (uint32_t i = 0; i < sourceCount; i++)
    {
        if (sources[i] != source)
        {
            continue;
        }

        memmove(&sources[i], &sources[i + 1], (sourceCount - i - 1) * sizeof(sys_time_source_t*));
        sourceCount--;

        if (bestSource == source)
        {
            bestSource = NULL;
            for (uint32_t j = 0; j < sourceCount; j++)
            {
                if (bestSource == NULL || sources[j]->precision < bestSource->precision)
                {
                    bestSource = sources[j];
                }
            }
        }

        rwlock_write_release(&sourcesLock);
        LOG_INFO("unregistered system timer source '%s'%s\n", source->name);
        return;
    }

    rwlock_write_release(&sourcesLock);
    panic(NULL, "Failed to unregister system timer source '%s', not found", source->name);
}

clock_t sys_time_uptime(void)
{
    RWLOCK_READ_SCOPE(&sourcesLock);
    if (bestSource == NULL)
    {
        return 0;
    }

    return bestSource->read();
}

time_t sys_time_unix_epoch(void)
{
    if (!bootEpochInitialized)
    {
        timer_boot_epoch_init();
    }

    return bootEpoch + sys_time_uptime() / CLOCKS_PER_SEC;
}

void sys_time_wait(clock_t nanoseconds)
{
    if (nanoseconds == 0)
    {
        return;
    }

    clock_t start = sys_time_uptime();
    if (start == 0)
    {
        panic(NULL, "sys_time_wait called before timer system initialized");
    }

    while (sys_time_uptime() - start < nanoseconds)
    {
        asm volatile("pause");
    }
}

SYSCALL_DEFINE(SYS_UPTIME, clock_t)
{
    return sys_time_uptime();
}

SYSCALL_DEFINE(SYS_UNIX_EPOCH, time_t, time_t* timePtr)
{
    time_t epoch = sys_time_unix_epoch();
    if (timePtr != NULL)
    {
        if (thread_copy_to_user(sched_thread(), timePtr, &epoch, sizeof(epoch)) == ERR)
        {
            return ERR;
        }
    }

    return epoch;
}
