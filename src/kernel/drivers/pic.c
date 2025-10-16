#include "pic.h"

#include "cpu/port.h"
#include "cpu/vectors.h"
#include "log/log.h"

void pic_disable(void)
{
    // We initialize the PIC before we then mask all interrupts.
    // Probably not needed but it ensures that the PIC is in a known state before we disable it.

    uint8_t a1 = port_inb(PIC1_DATA);
    port_wait();
    uint8_t a2 = port_inb(PIC2_DATA);
    port_wait();

    port_outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    port_wait();
    port_outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    port_wait();

    port_outb(PIC1_DATA, EXTERNAL_INTERRUPT_BASE);
    port_wait();
    port_outb(PIC2_DATA, EXTERNAL_INTERRUPT_BASE + 0x8);
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

    // Mask all interrupts.
    port_outb(PIC1_DATA, 0xFF);
    port_outb(PIC2_DATA, 0xFF);

    LOG_INFO("pic disabled\n");
}
