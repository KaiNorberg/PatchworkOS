#include <kernel/sched/clock.h>

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

static const clock_source_t* sources[CLOCK_MAX_SOURCES] = {0};
static uint32_t sourceCount = 0;
static const clock_source_t* bestNsSource = NULL;
static const clock_source_t* bestEpochSource = NULL;
static rwlock_t sourcesLock = RWLOCK_CREATE();

#ifdef DEBUG
static _Atomic(clock_t) lastNsTime = ATOMIC_VAR_INIT(0);
#endif

static void clock_update_best_sources(void)
{
    bestNsSource = NULL;
    bestEpochSource = NULL;

    for (uint32_t i = 0; i < sourceCount; i++)
    {
        const clock_source_t* source = sources[i];
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

status_t clock_source_register(const clock_source_t* source)
{
    if (source == NULL || (source->read_ns == NULL && source->read_epoch == NULL) || source->precision == 0)
    {
        return ERR(SCHED, INVAL);
    }

    rwlock_write_acquire(&sourcesLock);
    if (sourceCount >= CLOCK_MAX_SOURCES)
    {
        rwlock_write_release(&sourcesLock);
        return ERR(SCHED, MCLOCK);
    }

    sources[sourceCount++] = source;
    clock_update_best_sources();

    rwlock_write_release(&sourcesLock);

    LOG_INFO("registered system timer source '%s' with precision %lu ns\n", source->name, source->precision);
    return OK;
}

void clock_source_unregister(const clock_source_t* source)
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

        memmove(&sources[i], &sources[i + 1], (sourceCount - i - 1) * sizeof(clock_source_t*));
        sourceCount--;

        clock_update_best_sources();
        rwlock_write_release(&sourcesLock);
        return;
    }

    rwlock_write_release(&sourcesLock);
    panic(NULL, "Failed to unregister system timer source '%s', not found", source->name);
}

clock_t clock_uptime(void)
{
    RWLOCK_READ_SCOPE(&sourcesLock);
    if (bestNsSource == NULL)
    {
        return 0;
    }

    return bestNsSource->read_ns();
}

time_t clock_epoch(void)
{
    RWLOCK_READ_SCOPE(&sourcesLock);
    if (bestEpochSource == NULL)
    {
        return 0;
    }

    return bestEpochSource->read_epoch();
}

void clock_wait(clock_t nanoseconds)
{
    if (nanoseconds == 0)
    {
        return;
    }

    clock_t start = clock_uptime();
    if (start == 0)
    {
        panic(NULL, "clock_wait called before timer system initialized");
    }

    while (clock_uptime() - start < nanoseconds)
    {
        ASM("pause");
    }
}

SYSCALL_DEFINE(SYS_UPTIME)
{
    *_result = clock_uptime();
    return OK;
}

SYSCALL_DEFINE(SYS_TIME)
{
    *_result = clock_epoch();
    return OK;
}
