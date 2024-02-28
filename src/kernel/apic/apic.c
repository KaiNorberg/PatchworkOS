#include "apic.h"

#include "tty/tty.h"
#include "page_directory/page_directory.h"
#include "utils/utils.h"
#include "madt/madt.h"
#include "hpet/hpet.h"
#include "time/time.h"
#include "vmm/vmm.h"

static uintptr_t localApicBase;

void apic_init()
{
    tty_start_message("APIC initializing");

    localApicBase = (uintptr_t)vmm_map(madt_local_apic_address(), 1, PAGE_FLAG_WRITE);

    tty_end_message(TTY_MESSAGE_OK);
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

void local_apic_init()
{
    write_msr(MSR_LOCAL_APIC, (read_msr(MSR_LOCAL_APIC) | LOCAL_APIC_MSR_ENABLE) & ~(1 << 10));

    local_apic_write(LOCAL_APIC_REG_SPURIOUS, local_apic_read(LOCAL_APIC_REG_SPURIOUS) | 0x100);
}

uint8_t local_apic_id()
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

void local_apic_eoi()
{
    local_apic_write(LOCAL_APIC_REG_EOI, 0);
}