#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
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

void timer_cpu_ctx_init(timer_cpu_ctx_t* ctx)
{
    ctx->apicTicksPerNs = 0;
    ctx->nextDeadline = CLOCKS_NEVER;
    for (uint32_t i = 0; i < TIMER_MAX_CALLBACK; i++)
    {
        ctx->callbacks[i] = NULL;
    }
    lock_init(&ctx->lock);
}

void timer_interrupt_handler(interrupt_frame_t* frame, cpu_t* self)
{
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

void timer_register_callback(timer_cpu_ctx_t* ctx, timer_callback_t callback)
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
            LOG_DEBUG("timer callback %p registered in slot %d\n", callback, i);
            ctx->callbacks[i] = callback;
            return;
        }
    }

    panic(NULL, "Failed to register timer callback, no free slots available");
}

void timer_unregister_callback(timer_cpu_ctx_t* ctx, timer_callback_t callback)
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
            LOG_DEBUG("timer callback %p unregistered from slot %d\n", callback, i);
            ctx->callbacks[i] = NULL;
            return;
        }
    }

    panic(NULL, "Failed to unregister timer callback, not found");
}

void timer_one_shot(cpu_t* self, clock_t uptime, clock_t timeout)
{
    if (self == NULL || timeout == CLOCKS_NEVER)
    {
        return;
    }

    if (self->timer.apicTicksPerNs == 0)
    {
        self->timer.apicTicksPerNs = apic_timer_ticks_per_ns();
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

void timer_notify(cpu_t* cpu)
{
    lapic_send_ipi(cpu->lapicId, INTERRUPT_TIMER);
}

void timer_notify_self(void)
{
    asm volatile("int %0" : : "i"(INTERRUPT_TIMER));
}
