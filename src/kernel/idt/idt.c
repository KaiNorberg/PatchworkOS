#include "idt.h"

#include "io/io.h"
#include "tty/tty.h"

#include "interrupts/interrupts.h"
#include "page_allocator/page_allocator.h"

#define GDT_OFFSET_KERNEL_CODE 0x08

extern void* interrupt_vectors[256];

IdtEntry idt[256];

extern uint64_t syscall_interrupt;

void idt_init() 
{    
    tty_start_message("IDT initializing");

    for (uint16_t vector = 0; vector < 256; vector++) 
    {        
        idt_set_descriptor(vector, interrupt_vectors[vector], IDT_INTERRUPT);
    }

    remap_pic();

    io_outb(PIC1_DATA, 0b11111111);
    io_outb(PIC2_DATA, 0b11111111);

    io_pic_clear_mask(IRQ_CASCADE);

    IdtDesc idtDesc;
    idtDesc.size = (sizeof(IdtEntry) * 256) - 1;
    idtDesc.offset = (uint64_t)&idt;

    asm volatile ("lidt %0" : : "m"(idtDesc));

    tty_end_message(TTY_MESSAGE_OK);
}

void remap_pic()
{
    uint8_t a1 = io_inb(PIC1_DATA);
    io_wait();
    uint8_t a2 = io_inb(PIC2_DATA);
    io_wait();

    io_outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    io_outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    io_outb(PIC1_DATA, 0x20);
    io_wait();
    io_outb(PIC2_DATA, 0x28);
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
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) 
{
    IdtEntry* descriptor = &idt[vector];
 
    descriptor->isrLow = (uint64_t)isr & 0xFFFF;
    descriptor->codeSegment = GDT_OFFSET_KERNEL_CODE;
    descriptor->ist = 0;
    descriptor->attributes = flags;
    descriptor->isrMid = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->isrHigh = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->reserved = 0;
}

void enable_interrupts()
{    
    asm volatile ("sti");
}

void disable_interrupts()
{
    asm volatile ("cli");
}