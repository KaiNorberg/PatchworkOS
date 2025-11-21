#include <kernel/cpu/irq.h>
#include <kernel/drivers/pic.h>

#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/io.h>
#include <kernel/log/log.h>

static void pic_wait(void)
{
    io_out8(0x80, 0);
}

void pic_disable(void)
{
    // We initialize the PIC before we then mask all interrupts.
    // Probably not needed but it ensures that the PIC is in a known state before we disable it.

    uint8_t a1 = io_in8(PIC1_DATA);
    pic_wait();
    uint8_t a2 = io_in8(PIC2_DATA);
    pic_wait();

    io_out8(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    pic_wait();
    io_out8(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    pic_wait();

    io_out8(PIC1_DATA, IRQ_VIRT_EXTERNAL_START);
    pic_wait();
    io_out8(PIC2_DATA, IRQ_VIRT_EXTERNAL_START + 0x8);
    pic_wait();

    io_out8(PIC1_DATA, 4);
    pic_wait();
    io_out8(PIC2_DATA, 2);
    pic_wait();

    io_out8(PIC1_DATA, ICW4_8086);
    pic_wait();
    io_out8(PIC2_DATA, ICW4_8086);
    pic_wait();

    io_out8(PIC1_DATA, a1);
    pic_wait();
    io_out8(PIC2_DATA, a2);
    pic_wait();

    // Mask all interrupts.
    io_out8(PIC1_DATA, 0xFF);
    io_out8(PIC2_DATA, 0xFF);

    LOG_INFO("pic disabled\n");
}
