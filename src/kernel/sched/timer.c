#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/smp.h>
#include <kernel/cpu/syscalls.h>
#include <kernel/drivers/apic.h>
#include <kernel/drivers/hpet.h>
#include <kernel/drivers/rtc.h>
#include <kernel/log/log.h>
#include <kernel/sched/timer.h>

#include <kernel/log/panic.h>
#include <kernel/sched/thread.h>

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/math.h>

static time_t bootEpoch;
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
    lock_init(&ctx->lock);
    for (uint32_t i = 0; i < TIMER_MAX_CALLBACK; i++)
    {
        ctx->callbacks[i] = NULL;
    }
    LOG_INFO("cpu%d apic timer ticksPerNs=%lu\n", self->id, self->timer.apicTicksPerNs);
}

static void timer_init(void)
{
    struct tm time;
    rtc_read(&time);
    bootEpoch = mktime(&time);

    initialized = true;
}

clock_t timer_uptime(void)
{
    if (!initialized)
    {
        timer_init();
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
        timer_init();
    }

    return bootEpoch + timer_uptime() / CLOCKS_PER_SEC;
}

void timer_interrupt_handler(interrupt_frame_t* frame, cpu_t* self)
{
    timer_accumulate();

    LOCK_SCOPE(&self->timer.lock);
    self->timer.nextDeadline = CLOCKS_NEVER;
    for (uint32_t i = 0; i < TIMER_MAX_CALLBACK; i++)
    {
        if (self->timer.callbacks[i] != NULL)
        {
            self->timer.callbacks[i](frame, self);
        }
    }
}

void timer_subscribe(timer_ctx_t* ctx, timer_callback_t callback)
{
    if (ctx == NULL || callback == NULL)
    {
        return;
    }

    LOCK_SCOPE(&ctx->lock);
    for (uint32_t i = 0; i < TIMER_MAX_CALLBACK; i++)
    {
        if (ctx->callbacks[i] == NULL)
        {
            LOG_DEBUG("timer callback subscribed %p in slot %d\n", callback, i);
            ctx->callbacks[i] = callback;
            return;
        }
    }

    panic(NULL, "Failed to subscribe timer callback, no free slots available");
}

void timer_unsubscribe(timer_ctx_t* ctx, timer_callback_t callback)
{
    if (ctx == NULL || callback == NULL)
    {
        return;
    }

    LOCK_SCOPE(&ctx->lock);
    for (uint32_t i = 0; i < TIMER_MAX_CALLBACK; i++)
    {
        if (ctx->callbacks[i] == callback)
        {
            LOG_DEBUG("timer callback unsubscribed %p from slot %d\n", callback, i);
            ctx->callbacks[i] = NULL;
            return;
        }
    }

    panic(NULL, "Failed to unsubscribe timer callback, not found");
}

void timer_one_shot(cpu_t* self, clock_t uptime, clock_t timeout)
{
    if (self == NULL || timeout == CLOCKS_NEVER)
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
        apic_timer_one_shot(INTERRUPT_TIMER, (uint32_t)ticks);
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
        if (thread_copy_to_user(sched_thread(), timePtr, &epoch, sizeof(epoch)) == ERR)
        {
            return ERR;
        }
    }

    return epoch;
}

void timer_notify(cpu_t* cpu)
{
    lapic_send_ipi(cpu->lapicId, INTERRUPT_TIMER);
}

void timer_notify_self(void)
{
    asm volatile("int %0" : : "i"(INTERRUPT_TIMER));
}
