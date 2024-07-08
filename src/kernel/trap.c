#include "trap.h"

#include "apic.h"
#include "gdt.h"
#include "irq.h"
#include "log.h"
#include "regs.h"
#include "smp.h"

static void exception_handler(const trap_frame_t* trapFrame)
{
    switch (trapFrame->vector)
    {
    default:
    {
        log_panic(trapFrame, "Exception");
    }
    break;
    }
}

static void ipi_handler(const trap_frame_t* trapFrame)
{
    uint8_t ipi = trapFrame->vector - IPI_BASE;

    switch (ipi)
    {
    case IPI_HALT:
    {
        if (smp_initialized())
        {
            smp_halt_self();
        }
        else
        {
            while (1)
            {
                asm volatile("cli");
                asm volatile("hlt");
            }
        }
    }
    case IPI_START:
    {
        sched_cpu_start();
    }
    break;
    case IPI_SCHEDULE:
    {
        // Does nothing, scheduling is performed in vector_common
    }
    break;
    }

    lapic_eoi();
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
    // Race condition does not matter

    if (!smp_initialized())
    {
        return;
    }

    uint64_t rflags = rflags_read();

    asm volatile("cli");

    cpu_t* cpu = smp_self_unsafe();
    if (cpu->cliAmount == 0)
    {
        cpu->prevFlags = rflags;
    }
    cpu->cliAmount++;
}

void interrupts_enable(void)
{
    if (!smp_initialized())
    {
        return;
    }

    cpu_t* cpu = smp_self_unsafe();

    cpu->cliAmount--;
    if (cpu->cliAmount == 0 && cpu->prevFlags & RFLAGS_INTERRUPT_ENABLE)
    {
        asm volatile("sti");
    }
}

void trap_handler(trap_frame_t* trapFrame)
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
        log_panic(trapFrame, "Unknown interrupt vector");
    }

    thread_t* thread = sched_thread();
    if (thread != NULL && thread->process->killed && trapFrame->cs != GDT_KERNEL_CODE)
    {
        thread->state = THREAD_STATE_KILLED;
    }

    trap_end();
}
