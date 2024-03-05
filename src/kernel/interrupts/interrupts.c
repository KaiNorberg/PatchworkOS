#include "interrupts.h"

#include <stdint.h>

#include "irq/irq.h"
#include "tty/tty.h"
#include "apic/apic.h"
#include "debug/debug.h"
#include "vmm/vmm.h"
#include "page_directory/page_directory.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "smp/smp.h"
#include "scheduler/scheduler.h"

#include <libc/string.h>

static inline void exception_handler(InterruptFrame* interruptFrame)
{   
    switch (interruptFrame->vector)
    {
    default:
    {
        debug_panic("Exception");
    }
    break;
    }
}

static inline void ipi_handler(InterruptFrame* interruptFrame)
{
    Ipi ipi = smp_receive_ipi();

    switch (ipi.type)
    {
    case IPI_TYPE_HALT:
    {
        asm volatile("cli");
        while (1)
        {
            asm volatile("hlt");
        }
    }
    break;
    case IPI_TYPE_SCHEDULE:
    {
        //Does nothing, scheduler is invoked in interrupt_handler
    }
    break;
    }        
    
    local_apic_eoi();
}

void interrupt_handler(InterruptFrame* interruptFrame)
{
    smp_begin_interrupt();

    if (interruptFrame->vector < IRQ_BASE)
    {
        exception_handler(interruptFrame);
    }
    else if (interruptFrame->vector >= IRQ_BASE && interruptFrame->vector < IRQ_BASE + IRQ_AMOUNT)
    {
        irq_dispatch(interruptFrame);
    }
    else if (interruptFrame->vector == IPI_VECTOR)
    {
        ipi_handler(interruptFrame);
    }
    else
    {
        debug_panic("Unknown interrupt vector");
    }

    if (scheduler_wants_to_schedule())
    {
        scheduler_schedule(interruptFrame);
    }

    smp_end_interrupt();
}