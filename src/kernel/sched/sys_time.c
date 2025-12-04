#include <kernel/sched/sys_time.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/syscall.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/symbol.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>

#include <kernel/sync/rwlock.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/math.h>

static const sys_time_source_t* sources[SYS_TIME_MAX_SOURCES] = {0};
static uint32_t sourceCount = 0;
static const sys_time_source_t* bestNsSource = NULL;
static const sys_time_source_t* bestEpochSource = NULL;
static rwlock_t sourcesLock = RWLOCK_CREATE();

#ifdef DEBUG
static _Atomic(clock_t) lastNsTime = ATOMIC_VAR_INIT(0);
#endif

static void sys_time_update_best_sources(void)
{
    bestNsSource = NULL;
    bestEpochSource = NULL;

    for (uint32_t i = 0; i < sourceCount; i++)
    {
        const sys_time_source_t* source = sources[i];
        if (source->read_ns != NULL && (bestNsSource == NULL || source->precision < bestNsSource->precision))
        {
            bestNsSource = source;
        }
        if (source->read_epoch != NULL && (bestEpochSource == NULL || source->precision < bestEpochSource->precision))
        {
            bestEpochSource = source;
        }
    }
}

uint64_t sys_time_register_source(const sys_time_source_t* source)
{
    if (source == NULL || (source->read_ns == NULL && source->read_epoch == NULL) || source->precision == 0)
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

    sources[sourceCount++] = source;
    sys_time_update_best_sources();

    rwlock_write_release(&sourcesLock);

    LOG_INFO("registered system timer source '%s' with precision %lu ns\n", source->name, source->precision);
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

        sys_time_update_best_sources();
        rwlock_write_release(&sourcesLock);
        return;
    }

    rwlock_write_release(&sourcesLock);
    panic(NULL, "Failed to unregister system timer source '%s', not found", source->name);
}

clock_t sys_time_uptime(void)
{
    RWLOCK_READ_SCOPE(&sourcesLock);
    if (bestNsSource == NULL)
    {
        return 0;
    }

    clock_t time = bestNsSource->read_ns();
#ifdef DEBUG
    clock_t lastTime = atomic_exchange(&lastNsTime, time);
    if (time < lastTime)
    {
        panic(NULL, "system time source '%s' returned non-monotonic time value %lu ns (last %lu ns)",
            bestNsSource->name, time, lastTime);
    }
#endif
    return time;
}

time_t sys_time_unix_epoch(void)
{
    RWLOCK_READ_SCOPE(&sourcesLock);
    if (bestEpochSource == NULL)
    {
        return 0;
    }

    return bestEpochSource->read_epoch();
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
