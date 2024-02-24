#include "slow_timer.h"

#include <stdint.h>

#include "io/io.h"
#include "master/pic/pic.h"
#include "master/interrupts/interrupts.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

void slow_timer_init()
{
    io_outb(CMOS_ADDRESS, 0x8B);
    uint8_t temp = io_inb(CMOS_DATA);
    io_outb(CMOS_ADDRESS, 0x8B);
    io_outb(CMOS_DATA, temp | 0x40);    
    
    io_outb(CMOS_ADDRESS, 0x8A);
    temp = io_inb(CMOS_DATA);
    io_outb(CMOS_ADDRESS, 0x8A);
    io_outb(CMOS_DATA, (temp & 0xF0) | 15);

    pic_clear_mask(IRQ_SLOW_TIMER);
}

void slow_timer_eoi()
{    
    io_outb(CMOS_ADDRESS, 0x0C);
    io_inb(CMOS_DATA);
    
    pic_eoi(IRQ_SLOW_TIMER);
}