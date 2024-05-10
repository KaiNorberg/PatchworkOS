#include "apic.h"

#include "tty.h"
#include "regs.h"
#include "utils.h"
#include "madt.h"
#include "hpet.h"
#include "time.h"
#include "vmm.h"
#include "rsdt.h"
#include "pmm.h"

static uintptr_t localApicBase;

void apic_init(void)
{
    localApicBase = (uintptr_t)vmm_kernel_map(NULL, madt_local_apic_address(), PAGE_SIZE, PAGE_FLAG_WRITE);
}

void apic_timer_init(uint8_t vector, uint64_t hz)
{
    local_apic_write(LOCAL_APIC_REG_TIMER_DIVIDER, 0x3);
    local_apic_write(LOCAL_APIC_REG_TIMER_INITIAL_COUNT, 0xFFFFFFFF);

    hpet_nanosleep(NANOSECONDS_PER_SECOND / hz);

    local_apic_write(LOCAL_APIC_REG_LVT_TIMER, APIC_TIMER_MASKED);

    uint32_t ticks = 0xFFFFFFFF - local_apic_read(LOCAL_APIC_REG_TIMER_CURRENT_COUNT);

    local_apic_write(LOCAL_APIC_REG_LVT_TIMER, ((uint32_t)vector) | APIC_TIMER_PERIODIC);
    local_apic_write(LOCAL_APIC_REG_TIMER_DIVIDER, 0x3);
    local_apic_write(LOCAL_APIC_REG_TIMER_INITIAL_COUNT, ticks);
}

void local_apic_init(void)
{
    msr_write(MSR_LOCAL_APIC, (msr_read(MSR_LOCAL_APIC) | LOCAL_APIC_MSR_ENABLE) & ~(1 << 10));

    local_apic_write(LOCAL_APIC_REG_SPURIOUS, local_apic_read(LOCAL_APIC_REG_SPURIOUS) | 0x100);
}

uint8_t local_apic_id(void)
{
    return (uint8_t)(local_apic_read(LOCAL_APIC_REG_ID) >> LOCAL_APIC_ID_OFFSET);
}

void local_apic_write(uint32_t reg, uint32_t value)
{
    WRITE_32(localApicBase + reg, value);
}

uint32_t local_apic_read(uint32_t reg)
{
    return READ_32(localApicBase + reg);
}

void local_apic_send_init(uint32_t id)
{
    local_apic_write(LOCAL_APIC_REG_ICR1, id << LOCAL_APIC_ID_OFFSET);
    local_apic_write(LOCAL_APIC_REG_ICR0, (5 << 8));
}

void local_apic_send_sipi(uint32_t id, uint32_t page)
{
    local_apic_write(LOCAL_APIC_REG_ICR1, id << LOCAL_APIC_ID_OFFSET);
    local_apic_write(LOCAL_APIC_REG_ICR0, page | (6 << 8));
}

void local_apic_send_ipi(uint32_t id, uint8_t vector)
{
    local_apic_write(LOCAL_APIC_REG_ICR1, id << LOCAL_APIC_ID_OFFSET);
    local_apic_write(LOCAL_APIC_REG_ICR0, (uint32_t)vector | (1 << 14));
}

void local_apic_eoi(void)
{
    local_apic_write(LOCAL_APIC_REG_EOI, 0);
}