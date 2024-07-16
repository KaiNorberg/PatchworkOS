#include "trap.h"

#include "apic.h"
#include "gdt.h"
#include "irq.h"
#include "log.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "vectors.h"

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
    if (cpu->cliAmount == 0 && cpu->prevFlags & RFLAGS_INTERRUPT_ENABLE && cpu->trapDepth == 0)
    {
        asm volatile("sti");
    }
}

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

static void ipi_handler(trap_frame_t* trapFrame)
{
    cpu_t* cpu = smp_self_unsafe();
    while (1)
    {
        ipi_t ipi = smp_recieve(cpu);
        if (ipi == NULL)
        {
            break;
        }

        ipi(trapFrame);
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

void trap_handler(trap_frame_t* trapFrame)
{
    cpu_t* cpu = smp_self_unsafe();
    if (trapFrame->vector < IRQ_BASE)
    {
        exception_handler(trapFrame);
    }

    trap_begin();

    if (trapFrame->vector >= IRQ_BASE && trapFrame->vector < IRQ_BASE + IRQ_AMOUNT)
    {
        irq_dispatch(trapFrame);
    }
    else if (trapFrame->vector == VECTOR_IPI)
    {
        ipi_handler(trapFrame);
    }
    else if (trapFrame->vector == VECTOR_SCHED_TIMER)
    {
        lapic_eoi();
    }
    else if (trapFrame->vector == VECTOR_SCHED_INVOKE)
    {
        // Does nothing, scheduling happens in vector_common
    }
    else
    {
        log_panic(trapFrame, "Unknown vector");
    }

    trap_end();
}
