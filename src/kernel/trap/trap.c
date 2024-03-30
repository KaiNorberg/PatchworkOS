#include "trap.h"

#include "defs/defs.h"
#include "gdt/gdt.h"
#include "irq/irq.h"
#include "tty/tty.h"
#include "apic/apic.h"
#include "debug/debug.h"
#include "vmm/vmm.h"
#include "heap/heap.h"
#include "regs/regs.h"
#include "utils/utils.h"
#include "syscall/syscall.h"
#include "smp/smp.h"
#include "sched/schedule/schedule.h"

static inline void exception_handler(TrapFrame const* trapFrame)
{   
    switch (trapFrame->vector)
    {
    default:
    {
        debug_exception(trapFrame, "Exception");
    }
    break;
    }
}

static inline void ipi_handler(TrapFrame const* trapFrame)
{
    uint8_t ipi = trapFrame->vector - IPI_BASE;

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
        sched_cpu_start();
    }
    break;
    case IPI_SCHEDULE:
    {
        //Does nothing, scheduling is performed in vector_common
    }
    break;
    }
    
    local_apic_eoi();
}

void interrupts_disable(void)
{
    if (!smp_initialized())
    {
        return;
    }

    //Race condition does not matter
    uint64_t rflags = RFLAGS_READ();

    asm volatile("cli");
    
    Cpu* cpu = smp_self_unsafe();
    if (cpu->cliAmount == 0)
    {
        cpu->interruptsEnabled = (rflags & RFLAGS_INTERRUPT_ENABLE) != 0;
    }
    cpu->cliAmount++;
}

void interrupts_enable(void)
{
    if (!smp_initialized())
    {
        return;
    }

    Cpu* cpu = smp_self_unsafe();

    cpu->cliAmount--;
    if (cpu->cliAmount == 0 && cpu->interruptsEnabled)
    {    
        asm volatile("sti");
    }
}

void trap_handler(TrapFrame* trapFrame)
{
    Cpu* self = smp_self_unsafe();
    self->trapDepth++;

    if (trapFrame->vector < IRQ_BASE)
    {
        exception_handler(trapFrame);
    }
    else if (trapFrame->vector >= IRQ_BASE && trapFrame->vector < IRQ_BASE + IRQ_AMOUNT)
    {
        irq_dispatch(trapFrame);
    }
    else if (trapFrame->vector >= IPI_BASE && trapFrame->vector < IPI_BASE + IPI_AMOUNT)
    {
        ipi_handler(trapFrame);
    }
    else
    {
        debug_panic("Unknown interrupt vector");
    }

    Thread* thread = sched_thread();
    if (thread != NULL && thread->process->killed && trapFrame->cs != GDT_KERNEL_CODE)
    {
        thread->state = THREAD_STATE_KILLED;
    }

    self->trapDepth--;
}