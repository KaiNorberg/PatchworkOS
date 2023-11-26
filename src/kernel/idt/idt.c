#include "idt.h"

#include "kernel/io/io.h"

#include "interrupts/interrupts.h"

#define GDT_OFFSET_KERNEL_CODE 0x08

__attribute__((aligned(0x10))) 
IDTEntry idt[256];

IDTR idtr;

extern void* isr_stub_table[];
extern uint64_t syscall_interrupt;

void idt_init() 
{
    idtr.Base = (uint64_t)idt;
    idtr.Limit = 0x0FFF;
 
    for (uint8_t vector = 0; vector < 32; vector++) 
    {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
    }

    idt_set_descriptor(0x21, keyboard_interrupt, 0x8E);
    idt_set_descriptor(0x80, &syscall_interrupt, 0x8E);

    asm volatile ("lidt %0" : : "m"(idtr));

    remap_pic();

    io_outb(PIC1_DATA, 0b11111101);
    io_outb(PIC2_DATA, 0b11111111);
    asm volatile ("sti");
}

void remap_pic()
{
    //Remap pic
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

void exception_handler() 
{
    __asm__ volatile ("cli; hlt");
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) 
{
    IDTEntry* descriptor = &idt[vector];
 
    descriptor->IsrLow = (uint64_t)isr & 0xFFFF;
    descriptor->KernelCS = GDT_OFFSET_KERNEL_CODE;
    descriptor->Ist = 0;
    descriptor->Attributes = flags;
    descriptor->IsrMid = ((uint64_t)isr >> 16) & 0xFFFF;
    descriptor->IsrHigh = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
    descriptor->Reserved = 0;
}