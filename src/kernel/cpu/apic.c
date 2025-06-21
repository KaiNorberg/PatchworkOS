#include "apic.h"

#include "acpi/madt.h"
#include "drivers/systime/hpet.h"
#include "drivers/systime/systime.h"
#include "log/log.h"
#include "mem/vmm.h"
#include "regs.h"
#include "utils/utils.h"

#include <assert.h>

static uintptr_t lapicBase;

void apic_init(void)
{
    lapicBase = (uintptr_t)vmm_kernel_map(NULL, madt_lapic_address(), 1, PML_WRITE);
    assert((void*)lapicBase != NULL);
}

void apic_timer_one_shot(uint8_t vector, uint32_t ticks)
{
    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);
    lapic_write(LAPIC_REG_LVT_TIMER, ((uint32_t)vector) | APIC_TIMER_ONE_SHOT);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, ticks);
}

uint64_t apic_timer_ticks_per_ns(void)
{
    lapic_write(LAPIC_REG_TIMER_DIVIDER, APIC_TIMER_DEFAULT_DIV);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, UINT32_MAX);

    hpet_sleep(CLOCKS_PER_SEC / 1000);

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    uint64_t ticks = UINT32_MAX - lapic_read(LAPIC_REG_TIMER_CURRENT_COUNT);

    return (ticks << APIC_TIMER_TICKS_FIXED_POINT_OFFSET) / 1000000ULL;
}

void lapic_init(void)
{
    log_print(LOG_INFO, "lapic: init\n");
    msr_write(MSR_LAPIC, (msr_read(MSR_LAPIC) | LAPIC_MSR_ENABLE) & ~(1 << 10));

    lapic_write(LAPIC_REG_SPURIOUS, lapic_read(LAPIC_REG_SPURIOUS) | LAPIC_SPURIOUS_ENABLE);
}

uint8_t lapic_id(void)
{
    return (uint8_t)(lapic_read(LAPIC_REG_ID) >> LAPIC_ID_OFFSET);
}

void lapic_write(uint32_t reg, uint32_t value)
{
    WRITE_32(lapicBase + reg, value);
}

uint32_t lapic_read(uint32_t reg)
{
    return READ_32(lapicBase + reg);
}

void lapic_send_init(uint32_t id)
{
    lapic_write(LAPIC_REG_ICR1, id << LAPIC_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, (5 << 8));
}

void lapic_send_sipi(uint32_t id, uint32_t page)
{
    lapic_write(LAPIC_REG_ICR1, id << LAPIC_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, page | (6 << 8));
}

void lapic_send_ipi(uint32_t id, uint8_t vector)
{
    lapic_write(LAPIC_REG_ICR1, id << LAPIC_ID_OFFSET);
    lapic_write(LAPIC_REG_ICR0, (uint32_t)vector | (1 << 14));
}

void lapic_eoi(void)
{
    lapic_write(LAPIC_REG_EOI, 0);
}
