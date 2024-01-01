#include "apic.h"

#include "tty/tty.h"
#include "page_directory/page_directory.h"
#include "utils/utils.h"
#include "madt/madt.h"

uint64_t localApicBase;

void apic_init()
{
    tty_start_message("APIC initializing");

    localApicBase = madt_local_apic_address();
    page_directory_remap(kernelPageDirectory, (void*)localApicBase, (void*)localApicBase, PAGE_DIR_READ_WRITE);

    tty_end_message(TTY_MESSAGE_OK);
}

void local_apic_init()
{
    write_msr(MSR_REGISTER_LOCAL_APIC, (read_msr(MSR_REGISTER_LOCAL_APIC) | 0x800) & ~(1 << 10));

    local_apic_write(LOCAL_APIC_REGISTER_SPURIOUS, local_apic_read(LOCAL_APIC_REGISTER_SPURIOUS) | 0x100);
}

uint32_t local_apic_current_cpu()
{
    return local_apic_read(LOCAL_APIC_REGISTER_ID) >> LOCAL_APIC_ID_OFFSET;
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
    local_apic_write(LOCAL_APIC_REGISTER_ICR1, (uint64_t)localApicId << LOCAL_APIC_ID_OFFSET);
    local_apic_write(LOCAL_APIC_REGISTER_ICR0, (5 << 8));
}

void local_apic_send_sipi(uint32_t localApicId, uint32_t page)
{
    local_apic_write(LOCAL_APIC_REGISTER_ICR1, (uint64_t)localApicId << LOCAL_APIC_ID_OFFSET);
    local_apic_write(LOCAL_APIC_REGISTER_ICR0, page | (6 << 8));
}

void local_apic_send_ipi(uint32_t localApicId, uint64_t vector)
{
    local_apic_write(LOCAL_APIC_REGISTER_ICR1, (uint64_t)localApicId << LOCAL_APIC_ID_OFFSET);
    local_apic_write(LOCAL_APIC_REGISTER_ICR0, vector | (1 << 14));
}

void local_apic_eoi()
{
    local_apic_write(LOCAL_APIC_REGISTER_EOI, 0);
}