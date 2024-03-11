#include "interrupts.h"

#include <stdint.h>

#include "irq/irq.h"
#include "tty/tty.h"
#include "apic/apic.h"
#include "debug/debug.h"
#include "vmm/vmm.h"
#include "heap/heap.h"
#include "registers/registers.h"
#include "page_directory/page_directory.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "smp/smp.h"
#include "scheduler/scheduler.h"

#include <libc/string.h>

static InterruptState states[MAX_CPU_AMOUNT];

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
        /*asm volatile("cli");
        while (1)
        {
            asm volatile("hlt");
        }*/
    }
    break;
    case IPI_START:
    {
        scheduler_cpu_start();
    }
    break;
    case IPI_SCHEDULE:
    {
        //Does nothing, scheduling is performed in interrupt_handler
    }
    break;
    }
    
    local_apic_eoi();
}

static void interrupt_begin()
{
    states[smp_self_unsafe()->id].depth++;
}

static void interrupt_end()
{
    states[smp_self_unsafe()->id].depth--;
}

void interrupts_disable()
{
    //Race condition does not matter
    uint64_t rflags = rflags_read();

    asm volatile("cli");
    
    InterruptState* state = &states[smp_self_unsafe()->id];
    if (state->cliAmount == 0)
    {
        state->enabled = rflags & RFLAGS_INTERRUPT_ENABLE;
    }
    state->cliAmount++;
}

void interrupts_enable()
{
    InterruptState* state = &states[smp_self_unsafe()->id];

    state->cliAmount--;
    if (state->cliAmount == 0 && state->enabled)
    {    
        asm volatile("sti");
    }
}

uint64_t interrupt_depth()
{
    return states[smp_self_unsafe()->id].depth;
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

    interrupt_end();

    scheduler_schedule(interruptFrame);
}