#include "rtc.h"

#include "io/io.h"
#include "debug/debug.h"
#include "idt/idt.h"
#include "interrupts/interrupts.h"

uint64_t tick;

void rtc_init(uint8_t rate)
{
    io_outb(0x70, 0x8A);
    io_outb(0x71, 0x20);

    io_outb(0x70, 0x8B);
    char prev = io_inb(0x71);
    io_outb(0x70, 0x8B);
    io_outb(0x71, prev | 0x40);
    
    if (rate > 15) 
    {
        debug_panic("Attempted to set RTC to invalid rate!");
    }
    rate &= 0x0F;

    io_outb(0x70, 0x8A);
    prev = io_inb(0x71);
    io_outb(0x70, 0x8A);
    io_outb(0x71, (prev & 0xF0) | rate);

    io_outb(0x70, 0x0C);
    io_inb(0x71);

    tick = 0;

    io_pic_clear_mask(IRQ_CMOS);
}

uint64_t rtc_get_tick()
{
    return tick;
}

void rtc_tick()
{
    tick++;        
    
    io_outb(0x70, 0x0C);
    io_inb(0x71);
}