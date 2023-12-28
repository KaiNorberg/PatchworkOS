#include "apic.h"

#include "tty/tty.h"

uint64_t lapicBase;

void apic_init()
{
    tty_start_message("APIC initializing");

    MADT* madt = (MADT*)rsdt_lookup("APIC");
    if (madt == 0)
    {
        tty_print("Hardware is incompatible, unable to find MADT");
        tty_end_message(TTY_MESSAGE_ER);
    }

    lapicBase = madt->lapicAddress;

    tty_end_message(TTY_MESSAGE_OK);
}