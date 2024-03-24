#include "interrupts.h"

#include "types/types.h"
#include "gdt/gdt.h"
#include "irq/irq.h"
#include "tty/tty.h"
#include "apic/apic.h"
#include "debug/debug.h"
#include "vmm/vmm.h"
#include "heap/heap.h"
#include "registers/registers.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "smp/smp.h"
#include "scheduler/schedule/schedule.h"

static inline void exception_handler(InterruptFrame const* interruptFrame)
{   
    switch (interruptFrame->vector)
    {
    default:
    {
        debug_exception(interruptFrame, "Exception");
    }
    break;
    }
}

static inline void ipi_handler(InterruptFrame const* interruptFrame)
{
    uint8_t ipi = interruptFrame->vector - IPI_BASE;

    switch (ipi)
    {
    case IPI_HALT:
    {
        asm volatile("cli");
        while (1)
        {
            asm volatile("hlt");
        }
    }
    break;
    case IPI_START:
    {

        scheduler_cpu_start();
    }
    break;
    case IPI_SCHEDULE:
    {
        //Does nothing, scheduling is performed in common_vector
    }
    break;
    }
    
    local_apic_eoi();
}

static inline void interrupt_begin(void)
{
    smp_self_unsafe()->interruptDepth++;
}

static inline void interrupt_end(void)
{
    smp_self_unsafe()->interruptDepth--;
}

void interrupts_disable(void)
{
    if (smp_initialized())
    {
        //Race condition does not matter
        uint64_t rflags = rflags_read();

        asm volatile("cli");
        
        Cpu* cpu = smp_self_unsafe();
        if (cpu->cliAmount == 0)
        {
            cpu->interruptsEnabled = (rflags & RFLAGS_INTERRUPT_ENABLE) != 0;
        }
        cpu->cliAmount++;
    }
}

void interrupts_enable(void)
{
    if (smp_initialized())
    {
        Cpu* cpu = smp_self_unsafe();

        cpu->cliAmount--;
        if (cpu->cliAmount == 0 && cpu->interruptsEnabled)
        {    
            asm volatile("sti");
        }
    }
}

void interrupt_handler(InterruptFrame* interruptFrame)
{
    interrupt_begin();
    if (interruptFrame->vector < IRQ_BASE)
    {
        exception_handler(interruptFrame);
    }
    else if (interruptFrame->vector >= IRQ_BASE && interruptFrame->vector < IRQ_BASE + IRQ_AMOUNT)
    {
        irq_dispatch(interruptFrame);
    }
    else if (interruptFrame->vector >= IPI_BASE && interruptFrame->vector < IPI_BASE + IPI_AMOUNT)
    {
        ipi_handler(interruptFrame);
    }
    else
    {
        debug_panic("Unknown interrupt vector");
    }

    Thread* thread = scheduler_thread();
    if (thread != NULL && thread->process->killed && interruptFrame->codeSegment != GDT_KERNEL_CODE)
    {
        thread->state = THREAD_STATE_KILLED;
    }

    interrupt_end();
}