#include "apic.h"

#include "tty/tty.h"
#include "page_directory/page_directory.h"
#include "utils/utils.h"

uint64_t lapicBase;

void apic_init()
{
    tty_start_message("APIC initializing");

    Madt* madt = (Madt*)rsdt_lookup("APIC");
    if (madt == 0)
    {
        tty_print("Hardware is incompatible, unable to find Madt");
        tty_end_message(TTY_MESSAGE_ER);
    }

    lapicBase = madt->lapicAddress;
    page_directory_remap(kernelPageDirectory, (void*)lapicBase, (void*)lapicBase, PAGE_DIR_READ_WRITE);

    tty_end_message(TTY_MESSAGE_OK);
}

uint32_t lapic_current_cpu()
{
    return lapic_read(LAPIC_REGISTER_ID) >> 24;
}

void lapic_write(uint32_t reg, uint32_t value)
{
    WRITE_32(lapicBase + reg, value);
}

uint32_t lapic_read(uint32_t reg)
{
    return READ_32(lapicBase + reg);
}

void lapic_send_init(uint32_t cpu)
{
    lapic_write(LAPIC_REGISTER_ICR1, (uint64_t)cpu << 24);
    lapic_write(LAPIC_REGISTER_ICR0, (5 << 8));
}

void lapic_send_sipi(uint32_t cpu, uint32_t page)
{
    lapic_write(LAPIC_REGISTER_ICR1, (uint64_t)cpu << 24);
    lapic_write(LAPIC_REGISTER_ICR0, (6 << 8) | page);
}