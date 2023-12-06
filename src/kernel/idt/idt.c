#include "idt.h"

#include "kernel/io/io.h"
#include "kernel/tty/tty.h"

#include "interrupts/interrupts.h"

#define GDT_OFFSET_KERNEL_CODE 0x08

__attribute__((aligned(0x10))) 
IDTEntry idt[256];

IDTR idtr;

extern void* isr_stub_table[];
extern uint64_t syscall_interrupt;

void idt_init() 
{    
    tty_start_message("IDT initializing");

    idtr.Base = (uint64_t)idt;
    idtr.Limit = 0x0FFF;
 
    for (uint8_t vector = 0; vector < 32; vector++) 
    {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
    }

    idt_set_descriptor(0x0, device_by_zero_exception, 0x8E);
    idt_set_descriptor(0x2, none_maskable_interrupt_exception, 0x8E);
    idt_set_descriptor(0x3, breakpoint_exception, 0x8E);
    idt_set_descriptor(0x4, overflow_exception, 0x8E);
    idt_set_descriptor(0x5, boundRange_exception, 0x8E);
    idt_set_descriptor(0x6, invalid_opcode_exception, 0x8E);
    idt_set_descriptor(0x7, device_not_detected_exception, 0x8E);
    idt_set_descriptor(0x8, double_fault_exception, 0x8E);
    idt_set_descriptor(0xA, invalid_tts_exception, 0x8E);
    idt_set_descriptor(0xB, segment_not_present_exception, 0x8E);
    idt_set_descriptor(0xC, stack_segment_exception, 0x8E);
    idt_set_descriptor(0xD, general_protection_exception, 0x8E);
    idt_set_descriptor(0xE, page_fault_exception, 0x8E);
    idt_set_descriptor(0x10, floating_point_exception, 0x8E);

    idt_set_descriptor(0x21, keyboard_interrupt, 0x8E);
    idt_set_descriptor(0x80, &syscall_interrupt, 0x8E);

    asm volatile ("lidt %0" : : "m"(idtr));

    remap_pic();

    enable_irq();

    tty_end_message(TTY_MESSAGE_OK);
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

void enable_irq()
{
    io_outb(PIC1_DATA, 0b11111101);
    io_outb(PIC2_DATA, 0b11111111);
    asm volatile ("sti");
}

void disable_irq()
{
    io_outb(PIC1_DATA, 0b11111111);
    io_outb(PIC2_DATA, 0b11111111);
    asm volatile ("cli");
}