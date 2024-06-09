#include "pic.h"

#include "io.h"
#include "irq.h"

void pic_init(void)
{
    uint8_t a1 = io_inb(PIC1_DATA);
    io_wait();
    uint8_t a2 = io_inb(PIC2_DATA);
    io_wait();

    io_outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    io_outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    io_outb(PIC1_DATA, IRQ_BASE);
    io_wait();
    io_outb(PIC2_DATA, IRQ_BASE + 0x8);
    io_wait();

    io_outb(PIC1_DATA, 4);
    io_wait();
    io_outb(PIC2_DATA, 2);
    io_wait();

    io_outb(PIC1_DATA, ICW4_8086);
    io_wait();
    io_outb(PIC2_DATA, ICW4_8086);
    io_wait();

    io_outb(PIC1_DATA, a1);
    io_wait();
    io_outb(PIC2_DATA, a2);
    io_wait();

    io_outb(PIC1_DATA, 0x0);
    io_outb(PIC2_DATA, 0x0);
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8)
    {
        io_outb(PIC2_COMMAND, PIC_EOI);
    }

    io_outb(PIC1_COMMAND, PIC_EOI);
}

void pic_set_mask(uint8_t irq)
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
    uint8_t value = io_inb(port) | (uint8_t)(1 << irq);
    io_outb(port, value);
}

void pic_clear_mask(uint8_t irq)
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
    io_outb(port, value);
}