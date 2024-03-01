#include "interrupts.h"

#include <stdint.h>

#include "tty/tty.h"
#include "apic/apic.h"
#include "debug/debug.h"
#include "ipi/ipi.h"
#include "vmm/vmm.h"
#include "page_directory/page_directory.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "smp/smp.h"

void interrupt_handler(InterruptFrame* interruptFrame)
{            
    if (interruptFrame->vector < IRQ_BASE)
    {
        exception_handler(interruptFrame);
    }
    else if (interruptFrame->vector == SYSCALL_VECTOR)
    {
        syscall_handler(interruptFrame);
    }
    else if (interruptFrame->vector == IPI_VECTOR)
    {
        ipi_handler(interruptFrame);
    }
}

void ipi_handler(InterruptFrame* interruptFrame)
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
        /*Worker* worker = worker_self();

        scheduler_acquire(worker->scheduler);
        scheduler_schedule(worker->scheduler, interruptFrame);
        scheduler_release(worker->scheduler);*/
    }
    break;
    }        
    
    local_apic_eoi();
}

void exception_handler(InterruptFrame* interruptFrame)
{   
    switch (interruptFrame->errorCode)
    {
    default:
    {
        tty_acquire();
        debug_exception(interruptFrame, "Exception");
        tty_release();

        asm volatile("cli");
        while (1)
        {
            asm volatile("hlt");
        }
    }
    break;
    }
}