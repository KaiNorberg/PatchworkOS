#include "apic.h"

#include "hpet.h"
#include "madt.h"
#include "regs.h"
#include "systime.h"
#include "utils.h"
#include "vmm.h"

#include <stdio.h>

static uintptr_t lapicBase;

void apic_init(void)
{
    lapicBase = (uintptr_t)vmm_kernel_map(NULL, madt_lapic_address(), PAGE_SIZE);
}

void apic_timer_init(uint8_t vector, uint64_t hz)
{
    lapic_write(LAPIC_REG_TIMER_DIVIDER, 0x3);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, 0xFFFFFFFF);

    hpet_sleep(CLOCKS_PER_SEC / hz);

    lapic_write(LAPIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    uint32_t ticks = 0xFFFFFFFF - lapic_read(LAPIC_REG_TIMER_CURRENT_COUNT);

    lapic_write(LAPIC_REG_LVT_TIMER, ((uint32_t)vector) | APIC_TIMER_PERIODIC);
    lapic_write(LAPIC_REG_TIMER_DIVIDER, 0x3);
    lapic_write(LAPIC_REG_TIMER_INITIAL_COUNT, ticks);
}

void lapic_init(void)
{
    printf("lapic: init\n");
    msr_write(MSR_LAPIC, (msr_read(MSR_LAPIC) | LAPIC_MSR_ENABLE) & ~(1 << 10));

    lapic_write(LAPIC_REG_SPURIOUS, lapic_read(LAPIC_REG_SPURIOUS) | 0x100);
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
