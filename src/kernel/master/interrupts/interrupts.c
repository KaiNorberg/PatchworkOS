#include "interrupts.h"

#include "tty/tty.h"
#include "apic/apic.h"
#include "debug/debug.h"

#include "master/pic/pic.h"

extern void* masterVectorTable[IDT_VECTOR_AMOUNT];

void master_idt_populate(Idt* idt)
{
    for (uint16_t vector = 0; vector < IDT_VECTOR_AMOUNT; vector++) 
    {        
        idt_set_vector(idt, vector, masterVectorTable[vector], IDT_RING0, IDT_INTERRUPT_GATE);
    }
}

void master_interrupt_handler(InterruptFrame* interruptFrame)
{
    if (interruptFrame->vector < IRQ_BASE)
    {
        master_exception_handler(interruptFrame);
    }
    else if (interruptFrame->vector >= IRQ_BASE && interruptFrame->vector <= IRQ_BASE + IRQ_AMOUNT)
    {    
        master_irq_handler(interruptFrame);
    }
}

void master_exception_handler(InterruptFrame* interruptFrame)
{
    //TODO: Halt workers

    tty_acquire();
    debug_exception(interruptFrame, "Master Exception");
    tty_release();

    while (1)
    {
        asm volatile("hlt");
    }
}

void master_irq_handler(InterruptFrame* interruptFrame)
{    
    uint64_t irq = interruptFrame->vector - IRQ_BASE;

    switch (irq)
    {
    case IRQ_TIMER:
    {
        tty_acquire();
        tty_print("Timer\n\r");
        tty_release();

        local_apic_eoi();
    }
    break;
    }
}