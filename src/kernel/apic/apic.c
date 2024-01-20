#include "apic.h"

#include "tty/tty.h"
#include "page_directory/page_directory.h"
#include "utils/utils.h"
#include "madt/madt.h"
#include "hpet/hpet.h"
#include "time/time.h"
#include "smp/smp.h"

uint64_t localApicBase;

void apic_init()
{
    tty_start_message("APIC initializing");

    localApicBase = madt_local_apic_address();
    page_directory_remap(kernelPageDirectory, (void*)localApicBase, (void*)localApicBase, PAGE_DIR_READ_WRITE);

    tty_end_message(TTY_MESSAGE_OK);
}

void apic_timer_init()
{
    local_apic_write(APIC_REGISTER_TIMER_DIVIDER, 0x3);
    local_apic_write(APIC_REGISTER_TIMER_INITIAL_COUNT, 0xFFFFFFFF);

    hpet_sleep(1000 / APIC_TIMER_HZ);

    local_apic_write(APIC_REGISTER_LVT_TIMER, APIC_TIMER_MASKED);

    uint64_t ticks = 0xFFFFFFFF - local_apic_read(APIC_REGISTER_TIMER_CURRENT_COUNT);

    local_apic_write(APIC_REGISTER_LVT_TIMER, 0x20 | APIC_TIMER_PERIODIC);
    local_apic_write(APIC_REGISTER_TIMER_DIVIDER, 0x3);    
    local_apic_write(APIC_REGISTER_TIMER_INITIAL_COUNT, ticks);
}

void local_apic_init()
{
    write_msr(MSR_REGISTER_LOCAL_APIC, (read_msr(MSR_REGISTER_LOCAL_APIC) | 0x800) & ~(1 << 10));

    local_apic_write(APIC_REGISTER_SPURIOUS, local_apic_read(APIC_REGISTER_SPURIOUS) | 0x100);
}

uint32_t local_apic_current_cpu()
{
    return local_apic_read(APIC_REGISTER_ID) >> LOCAL_APIC_ID_OFFSET;
}

void local_apic_write(uint32_t reg, uint32_t value)
{
    WRITE_32(localApicBase + reg, value);
}

uint32_t local_apic_read(uint32_t reg)
{
    return READ_32(localApicBase + reg);
}

void local_apic_send_init(uint32_t localApicId)
{
    local_apic_write(APIC_REGISTER_ICR1, (uint64_t)localApicId << LOCAL_APIC_ID_OFFSET);
    local_apic_write(APIC_REGISTER_ICR0, (5 << 8));
}

void local_apic_send_sipi(uint32_t localApicId, uint32_t page)
{
    local_apic_write(APIC_REGISTER_ICR1, (uint64_t)localApicId << LOCAL_APIC_ID_OFFSET);
    local_apic_write(APIC_REGISTER_ICR0, page | (6 << 8));
}

void local_apic_send_ipi(uint32_t localApicId, uint64_t vector)
{
    local_apic_write(APIC_REGISTER_ICR1, (uint64_t)localApicId << LOCAL_APIC_ID_OFFSET);
    local_apic_write(APIC_REGISTER_ICR0, vector | (1 << 14));
}

void local_apic_eoi()
{
    local_apic_write(APIC_REGISTER_EOI, 0);
}