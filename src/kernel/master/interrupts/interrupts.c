#include "interrupts.h"

#include <stdint.h>

#include "tty/tty.h"
#include "debug/debug.h"
#include "worker_pool/worker_pool.h"
#include "worker/worker.h"
#include "master/dispatcher/dispatcher.h"
#include "master/fast_timer/fast_timer.h"
#include "master/pic/pic.h"
#include "master/slow_timer/slow_timer.h"
#include "ipi/ipi.h"

extern void* masterVectorTable[IDT_VECTOR_AMOUNT];

static Idt idt;

void master_idt_init()
{
    for (uint16_t vector = 0; vector < IDT_VECTOR_AMOUNT; vector++) 
    {        
        idt_set_vector(&idt, (uint8_t)vector, masterVectorTable[vector], IDT_RING0, IDT_INTERRUPT_GATE);
    }
}

void master_interrupt_handler(InterruptFrame const* interruptFrame)
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

void master_exception_handler(InterruptFrame const* interruptFrame)
{
    Ipi ipi = 
    {
        .type = IPI_WORKER_HALT
    };
    worker_pool_send_ipi(ipi);

    tty_acquire();
    debug_exception(interruptFrame, "Master Exception");
    tty_release();

    asm volatile("cli");
    while (1)
    {
        asm volatile("hlt");
    }
}

void master_irq_handler(InterruptFrame const* interruptFrame)
{    
    uint8_t irq = (uint8_t)interruptFrame->vector - IRQ_BASE;

    dispatcher_dispatch(irq);

    switch (irq)
    {
    case IRQ_FAST_TIMER:
    {
        fast_timer_eoi();
    }
    break;
    case IRQ_SLOW_TIMER:
    {        
        slow_timer_eoi();
    }
    break;
    default:
    {
        pic_eoi(irq);
    }
    break;
    }
}