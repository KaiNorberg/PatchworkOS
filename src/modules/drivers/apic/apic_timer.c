#include <modules/drivers/apic/apic_timer.h>
#include <modules/drivers/apic/lapic.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/irq.h>
#include <kernel/log/log.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/timer.h>
#include <kernel/utils/utils.h>

#include <kernel/defs.h>
#include <stdint.h>

static uint64_t apic_timer_ticks_per_ms(void)
{
    interrupt_disable();

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DIV_DEFAULT);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, UINT32_MAX);

    sys_time_wait(CLOCKS_PER_SEC / 1000);

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    uint64_t ticks = UINT32_MAX - lapic_read(LAPIC_REG_TIMER_CURRENT_COUNT);
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, 0);

    interrupt_enable();
    return ticks;
}

static void apic_timer_set(irq_virt_t virt, clock_t uptime, clock_t timeout)
{
    (void)uptime;

    lapic_t* lapic = lapic_get(cpu_get_id_unsafe());
    if (lapic->ticksPerMs == 0)
    {
        lapic->ticksPerMs = apic_timer_ticks_per_ms();
    }

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, 0);

    if (timeout == CLOCKS_NEVER)
    {
        return;
    }

    uint32_t ticks = (timeout * lapic->ticksPerMs) / (CLOCKS_PER_SEC / 1000);
    if (ticks == 0)
    {
        ticks = 1;
    }

    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DIV_DEFAULT);
    lapic_write(LAPIC_REG_LVT_TIMER, ((uint32_t)virt) | APIC_TIMER_ONE_SHOT);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, ticks);
}

static void apic_timer_eoi(cpu_t* cpu)
{
    (void)cpu;

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

uint64_t apic_timer_init(void)
{
    if (timer_source_register(&apicTimer) == ERR)
    {
        LOG_ERR("failed to register apic timer source\n");
        return ERR;
    }

    return 0;
}
