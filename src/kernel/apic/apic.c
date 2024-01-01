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

uint32_t local_apic_current_cpu()
{
    return local_apic_read(LOCAL_APIC_REGISTER_ID) >> LOCAL_APIC_REGISTER_ID_CPU_OFFSET;
}

void local_apic_write(uint32_t reg, uint32_t value)
{
    WRITE_32(localApicBase + reg, value);
}

uint32_t local_apic_read(uint32_t reg)
{
    return READ_32(localApicBase + reg);
}

void local_apic_send_init(uint32_t cpu)
{
    local_apic_write(LOCAL_APIC_REGISTER_ICR1, (uint64_t)cpu << LOCAL_APIC_REGISTER_ID_CPU_OFFSET);
    local_apic_write(LOCAL_APIC_REGISTER_ICR0, (5 << 8));
}

void local_apic_send_sipi(uint32_t cpu, uint32_t page)
{
    local_apic_write(LOCAL_APIC_REGISTER_ICR1, (uint64_t)cpu << LOCAL_APIC_REGISTER_ID_CPU_OFFSET);
    local_apic_write(LOCAL_APIC_REGISTER_ICR0, (6 << 8) | page);
}