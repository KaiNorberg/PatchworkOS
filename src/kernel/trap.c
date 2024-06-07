#include "trap.h"

#include "defs.h"
#include "gdt.h"
#include "irq.h"
#include "tty.h"
#include "apic.h"
#include "debug.h"
#include "vmm.h"
#include "regs.h"
#include "utils.h"
#include "syscall.h"
#include "smp.h"

static void exception_handler(TrapFrame const* trapFrame)
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

static void ipi_handler(TrapFrame const* trapFrame)
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

static void trap_begin(void)
{
    smp_self_unsafe()->trapDepth++;
}

static void trap_end(void)
{
    smp_self_unsafe()->trapDepth--;
}

void interrupts_disable(void)
{
    if (!smp_initialized())
    {
        return;
    }

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
    if (trapFrame->vector < IRQ_BASE)
    {
        exception_handler(trapFrame);
    }

    trap_begin();

    if (trapFrame->vector >= IRQ_BASE && trapFrame->vector < IRQ_BASE + IRQ_AMOUNT)
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

    trap_end();
}