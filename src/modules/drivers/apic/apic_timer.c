#include <modules/drivers/apic/apic_timer.h>
#include <modules/drivers/apic/lapic.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>
#include <kernel/utils/utils.h>

#include <stdint.h>
#include <sys/defs.h>

static uint64_t apic_timer_ticks_per_ms(void)
{
    CLI_SCOPE();

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DIV_DEFAULT);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, UINT32_MAX);

    clock_wait(CLOCKS_PER_MS);

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    uint64_t ticks = UINT32_MAX - lapic_read(LAPIC_REG_TIMER_CURRENT_COUNT);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, 0);

    return ticks;
}

static void apic_timer_set(irq_virt_t virt, clock_t uptime, clock_t timeout)
{
    UNUSED(uptime);

    CLI_SCOPE();

    if (_pcpu_lapic->ticksPerMs == 0)
    {
        _pcpu_lapic->ticksPerMs = apic_timer_ticks_per_ms();
    }

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, 0);

    if (timeout == CLOCKS_NEVER)
    {
        return;
    }

    uint64_t ticks = (timeout * _pcpu_lapic->ticksPerMs) / (CLOCKS_PER_MS);
    if (ticks == 0)
    {
        ticks = 1;
    }
    if (ticks > UINT32_MAX)
    {
        ticks = UINT32_MAX;
    }

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DIV_DEFAULT);
    lapic_write(LAPIC_REG_LVT_TIMER, ((uint32_t)virt) | APIC_TIMER_ONE_SHOT);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, ticks);
}

static void apic_timer_eoi(void)
{
    lapic_write(LAPIC_REG_EOI, 0);
}

/**
 * According to https://telematics.tm.kit.edu/publications/Files/61/walter_ibm_linux_challenge.pdf, the APIC timer has a
 * precision of 1 microsecond.
 */
static timer_source_t apicTimer = {
    .name = "APIC Timer",
    .precision = 1000, // 1 microsecond
    .set = apic_timer_set,
    .ack = NULL,
    .eoi = apic_timer_eoi,
};

CONSTRUCTOR(102) static uint64_t apic_timer_init(void)
{
    if (timer_source_register(&apicTimer) == ERR)
    {
        LOG_ERR("failed to register apic timer source\n");
        return ERR;
    }

    return 0;
}
