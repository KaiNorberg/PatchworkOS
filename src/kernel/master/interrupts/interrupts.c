#include "interrupts.h"

#include "tty/tty.h"
#include "apic/apic.h"
#include "debug/debug.h"
#include "worker_pool/worker_pool.h"
#include "hpet/hpet.h"

#include "worker/worker.h"
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

void master_irq_handler(InterruptFrame* interruptFrame)
{    
    uint64_t irq = interruptFrame->vector - IRQ_BASE;

    switch (irq)
    {
    case IRQ_TIMER:
    {             
        time_tick();
   
        //Temporary for testing
        tty_acquire();
        Point cursorPos = tty_get_cursor_pos();
        tty_set_cursor_pos(0, 0);
        tty_print("MASTER | TIME: "); 
        tty_printx(time_nanoseconds()); 
        tty_set_cursor_pos(cursorPos.x, cursorPos.y);
        tty_release();

        for (uint16_t i = 0; i < worker_amount(); i++)
        {
            Worker* worker = worker_get(i);

            scheduler_acquire(worker->scheduler);

            scheduler_unblock(worker->scheduler);

            if (scheduler_wants_to_schedule(worker->scheduler))
            {
                Ipi ipi = 
                {
                    .type = IPI_WORKER_SCHEDULE
                };
                worker_send_ipi(worker, ipi);
            }

            scheduler_release(worker->scheduler);
        }

        local_apic_eoi();
    }
    break;
    }
}