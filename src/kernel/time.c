#include "time.h"

#include <stdatomic.h>

#include "apic.h"
#include "hpet.h"
#include "io.h"
#include "irq.h"
#include "pic.h"
#include "splash.h"

static _Atomic(nsec_t) accumulator = ATOMIC_VAR_INIT(0);

static void time_accumulate(void)
{
    // Avoids overflow on the hpet counter if counter is 32bit.
    atomic_fetch_add(&accumulator, hpet_read_counter());
    hpet_reset_counter();
}

static void time_irq_handler(uint8_t irq)
{
    time_accumulate();

    io_outb(CMOS_ADDRESS, 0x0C);
    io_inb(CMOS_DATA);
}

static void time_rtc_init(void)
{
    irq_install(time_irq_handler, IRQ_CMOS);

    io_outb(CMOS_ADDRESS, 0x8B);
    uint8_t temp = io_inb(CMOS_DATA);
    io_outb(CMOS_ADDRESS, 0x8B);
    io_outb(CMOS_DATA, temp | 0x40);

    io_outb(CMOS_ADDRESS, 0x8A);
    temp = io_inb(CMOS_DATA);
    io_outb(CMOS_ADDRESS, 0x8A);
    io_outb(CMOS_DATA, (temp & 0xF0) | 15);
}

void time_init(void)
{
    time_accumulate();

    time_rtc_init();
}

nsec_t time_uptime(void)
{
    return (atomic_load(&accumulator) + hpet_read_counter()) * hpet_nanoseconds_per_tick();
}
