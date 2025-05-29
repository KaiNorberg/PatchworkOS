#include "pic.h"

#include "irq.h"
#include "port.h"
#include "vectors.h"

void pic_init(void)
{
    uint8_t a1 = port_inb(PIC1_DATA);
    port_wait();
    uint8_t a2 = port_inb(PIC2_DATA);
    port_wait();

    port_outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    port_wait();
    port_outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    port_wait();

    port_outb(PIC1_DATA, VECTOR_IRQ_BASE);
    port_wait();
    port_outb(PIC2_DATA, VECTOR_IRQ_BASE + 0x8);
    port_wait();

    port_outb(PIC1_DATA, 4);
    port_wait();
    port_outb(PIC2_DATA, 2);
    port_wait();

    port_outb(PIC1_DATA, ICW4_8086);
    port_wait();
    port_outb(PIC2_DATA, ICW4_8086);
    port_wait();

    port_outb(PIC1_DATA, a1);
    port_wait();
    port_outb(PIC2_DATA, a2);
    port_wait();

    port_outb(PIC1_DATA, 0xFF);
    port_outb(PIC2_DATA, 0xFF);

    pic_clear_mask(IRQ_CASCADE);
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8)
    {
        port_outb(PIC2_COMMAND, PIC_EOI);
    }

    port_outb(PIC1_COMMAND, PIC_EOI);
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
    uint8_t value = port_inb(port) | (uint8_t)(1 << irq);
    port_outb(port, value);
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
    uint8_t value = port_inb(port) & ~(1 << irq);
    port_outb(port, value);
}
