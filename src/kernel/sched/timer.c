#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/irq.h>
#include <kernel/cpu/syscall.h>
#include <kernel/drivers/rtc.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/symbol.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>

#include <kernel/sync/rwlock.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/math.h>
#include <time.h>

static const timer_source_t* sources[TIMER_MAX_SOURCES] = {0};
static uint32_t sourceCount = 0;
static const timer_source_t* bestSource = NULL;
static rwlock_t sourcesLock = RWLOCK_CREATE;

void timer_cpu_ctx_init(timer_cpu_ctx_t* ctx)
{
    ctx->deadline = CLOCKS_NEVER;
}

void timer_ack_eoi(interrupt_frame_t* frame, cpu_t* self)
{
    (void)frame; // Unused

    RWLOCK_READ_SCOPE(&sourcesLock);

    self->timer.deadline = CLOCKS_NEVER;

    if (bestSource != NULL)
    {
        if (bestSource->ack != NULL)
        {
            bestSource->ack(self);
        }
        if (bestSource->eoi != NULL)
        {
            bestSource->eoi(self);
        }
    }
}

uint64_t timer_source_register(const timer_source_t* source)
{
    if (source == NULL || source->set == NULL || source->precision == 0 || source->name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    rwlock_write_acquire(&sourcesLock);
    if (sourceCount >= TIMER_MAX_SOURCES)
    {
        rwlock_write_release(&sourcesLock);
        errno = ENOSPC;
        return ERR;
    }

    for (uint32_t i = 0; i < sourceCount; i++)
    {
        if (sources[i] == source)
        {
            rwlock_write_release(&sourcesLock);
            return 0;
        }
    }

    sources[sourceCount++] = source;

    bool bestSourceUpdated = false;
    if (bestSource == NULL || source->precision < bestSource->precision)
    {
        bestSource = source;
        bestSourceUpdated = true;
    }
    rwlock_write_release(&sourcesLock);

    LOG_INFO("registered timer source '%s'%s\n", source->name, bestSourceUpdated ? " (best source)" : "");
    return 0;
}

void timer_source_unregister(const timer_source_t* source)
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

        memmove(&sources[i], &sources[i + 1], (sourceCount - i - 1) * sizeof(timer_source_t*));
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
        LOG_INFO("unregistered timer source '%s'\n", source->name);
        return;
    }

    rwlock_write_release(&sourcesLock);
}

uint64_t timer_source_amount(void)
{
    rwlock_read_acquire(&sourcesLock);
    uint64_t amount = sourceCount;
    rwlock_read_release(&sourcesLock);
    return amount;
}

void timer_set(clock_t uptime, clock_t deadline)
{
    if (deadline == CLOCKS_NEVER)
    {
        return;
    }

    RWLOCK_READ_SCOPE(&sourcesLock);

    cpu_t* cpu = cpu_get_unsafe();
    if (cpu->timer.deadline < deadline)
    {
        return;
    }
    cpu->timer.deadline = deadline;

    if (bestSource != NULL)
    {
        clock_t timeout;
        if (deadline <= uptime)
        {
            timeout = CONFIG_MIN_TIMER_TIMEOUT;
        }
        else
        {
            timeout = deadline - uptime;
        }

        bestSource->set(VECTOR_TIMER, uptime, timeout);
    }
}