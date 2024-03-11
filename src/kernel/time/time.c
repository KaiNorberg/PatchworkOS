#include "time.h"

#include <stdatomic.h>

#include "irq/irq.h"
#include "io/io.h"
#include "pic/pic.h"
#include "tty/tty.h"
#include "apic/apic.h"
#include "hpet/hpet.h"

static _Atomic uint64_t accumulator;

static void time_irq_handler(uint8_t irq)
{
    time_accumulate();    
    
    io_outb(CMOS_ADDRESS, 0x0C);
    io_inb(CMOS_DATA);
}

static void time_rtc_init()
{
    irq_install_handler(time_irq_handler, IRQ_CMOS);

    /*io_outb(CMOS_ADDRESS, 0x8B);
    uint8_t temp = io_inb(CMOS_DATA);
    io_outb(CMOS_ADDRESS, 0x8B);
    io_outb(CMOS_DATA, temp | 0x40);    
    
    io_outb(CMOS_ADDRESS, 0x8A);
    temp = io_inb(CMOS_DATA);
    io_outb(CMOS_ADDRESS, 0x8A);
    io_outb(CMOS_DATA, (temp & 0xF0) | 15);

    //TODO: Implement io apic
    pic_clear_mask(IRQ_CASCADE);
    pic_clear_mask(IRQ_CMOS);*/
}

void time_init()
{
    accumulator = 0;
    time_accumulate();

    time_rtc_init();
}

void time_accumulate()
{
    //Avoids overflow on the hpet counter if counter is 32bit.
    atomic_fetch_add(&accumulator, hpet_read_counter());
    hpet_reset_counter();
}

uint64_t time_seconds()
{
    return time_nanoseconds() / NANOSECONDS_PER_SECOND;
}

uint64_t time_milliseconds()
{
    return time_nanoseconds() / NANOSECONDS_PER_MILLISECOND;
}

uint64_t time_nanoseconds()
{
    return (atomic_load(&accumulator) + hpet_read_counter()) * hpet_nanoseconds_per_tick();
}
