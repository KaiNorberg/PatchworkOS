#include "io.h"

void io_outb(uint16_t port, uint8_t val)
{
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

uint8_t io_inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

void io_wait()
{
    io_outb(0x80, 0);
}

void io_pic_eoi(uint8_t irq)
{
	if (irq >= 8)
    {
        io_outb(PIC2_COMMAND, PIC_EOI);
    }

	io_outb(PIC1_COMMAND, PIC_EOI);
}

void io_pic_set_mask(uint8_t irq) 
{
    uint16_t port;
    if (irq < 8) 
    {
        port = PIC1_DATA;
    } 
    else 
    {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = io_inb(port) | (1 << irq);
    io_wait();
    io_outb(port, value);        
}
 
void io_pic_clear_mask(uint8_t irq)
{
    uint16_t port;
    if (irq < 8) 
    {
        port = PIC1_DATA;
    } 
    else 
    {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = io_inb(port) & ~(1 << irq);
    io_wait();
    io_outb(port, value);        
}