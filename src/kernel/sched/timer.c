#include "timer.h"
#include "cpu/cpu.h"
#include "cpu/smp.h"
#include "cpu/syscalls.h"
#include "cpu/vectors.h"
#include "drivers/apic.h"
#include "drivers/hpet.h"
#include "drivers/rtc.h"
#include "log/log.h"
#include "log/panic.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/math.h>

static time_t bootEpoch;

static timer_callback_t callbacks[TIMER_MAX_CALLBACK] = {0};

static bool initialized = false;

static atomic_int8_t accumulatorLock = ATOMIC_VAR_INIT(0);
static clock_t accumulator = 0;

static void timer_acquire(void)
{
    // We cant use the lock_t here becouse in debug mode lock_t will use the timer to check for deadlocks.
    interrupt_disable();
    while (!atomic_compare_exchange_strong(&accumulatorLock, &(int8_t){0}, 1))
    {
        asm volatile("pause");
    }
}

static void timer_release(void)
{
    atomic_store(&accumulatorLock, 0);
    interrupt_enable();
}

static void timer_accumulate(void)
{
    timer_acquire();

    accumulator += hpet_read_counter() * hpet_nanoseconds_per_tick();
    hpet_reset_counter();

    timer_release();
}

void timer_ctx_init(timer_ctx_t* ctx)
{
    cpu_t* self = smp_self_unsafe();
    ctx->apicTicksPerNs = apic_timer_ticks_per_ns();
    ctx->nextDeadline = CLOCKS_NEVER;
    LOG_INFO("cpu%d apic timer ticksPerNs=%lu\n", self->id, self->timer.apicTicksPerNs);
}

void timer_init(void)
{
    struct tm time;
    rtc_read(&time);
    bootEpoch = mktime(&time);

    initialized = true;
    LOG_INFO("timer initialized epoch=%d\n", timer_unix_epoch());
}

clock_t timer_uptime(void)
{
    if (!initialized)
    {
        return 0;
    }

    timer_acquire();
    clock_t time = accumulator + hpet_read_counter() * hpet_nanoseconds_per_tick();
    timer_release();
    return time;
}

time_t timer_unix_epoch(void)
{
    if (!initialized)
    {
        return 0;
    }

    return bootEpoch + timer_uptime() / CLOCKS_PER_SEC;
}

void timer_interrupt_handler(interrupt_frame_t* frame, cpu_t* self)
{
    timer_accumulate();

    self->timer.nextDeadline = CLOCKS_NEVER;
    for (uint32_t i = 0; i < TIMER_MAX_CALLBACK; i++)
    {
        if (callbacks[i] != NULL)
        {
            callbacks[i](frame, self);
        }
    }
}

void timer_subscribe(timer_callback_t callback)
{
    if (callback == NULL)
    {
        return;
    }

    for (uint32_t i = 0; i < TIMER_MAX_CALLBACK; i++)
    {
        if (callbacks[i] == NULL)
        {
            LOG_DEBUG("timer callback subscribed %p in slot %d\n", callback, i);
            callbacks[i] = callback;
            return;
        }
    }

    panic(NULL, "Failed to subscribe timer callback, no free slots available");
}

void timer_unsubscribe(timer_callback_t callback)
{
    if (callback == NULL)
    {
        return;
    }

    for (uint32_t i = 0; i < TIMER_MAX_CALLBACK; i++)
    {
        if (callbacks[i] == callback)
        {
            LOG_DEBUG("timer callback unsubscribed %p from slot %d\n", callback, i);
            callbacks[i] = NULL;
            return;
        }
    }

    panic(NULL, "Failed to unsubscribe timer callback, not found");
}

void timer_one_shot(cpu_t* self, clock_t uptime, clock_t timeout)
{
    if (timeout == CLOCKS_NEVER)
    {
        return;
    }

    clock_t deadline = uptime + timeout;
    if (deadline < self->timer.nextDeadline)
    {
        uint64_t ticks = (timeout * self->timer.apicTicksPerNs) >> APIC_TIMER_TICKS_FIXED_POINT_OFFSET;
        if (ticks > UINT32_MAX)
        {
            ticks = UINT32_MAX;
        }
        else if (ticks == 0)
        {
            ticks = 1;
        }

        self->timer.nextDeadline = deadline;
        apic_timer_one_shot(VECTOR_TIMER, (uint32_t)ticks);
    }
}

SYSCALL_DEFINE(SYS_UPTIME, clock_t)
{
    return timer_uptime();
}

SYSCALL_DEFINE(SYS_UNIX_EPOCH, time_t, time_t* timePtr)
{
    time_t epoch = timer_unix_epoch();
    if (timePtr != NULL)
    {
        if (!syscall_is_pointer_valid(timePtr, sizeof(time_t)))
        {
            errno = EFAULT;
            return ERR;
        }

        *timePtr = epoch;
    }

    return epoch;
}

void timer_notify(cpu_t* cpu)
{
    lapic_send_ipi(cpu->lapicId, VECTOR_TIMER);
}

void timer_notify_self(void)
{
    asm volatile("int %0" : : "i"(VECTOR_TIMER));
}
