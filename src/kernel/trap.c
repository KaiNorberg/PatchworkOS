#include "trap.h"

#include "apic.h"
#include "gdt.h"
#include "irq.h"
#include "log.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "vectors.h"

void cli_push(void)
{
    if (!smp_initialized())
    {
        return;
    }

    // Race condition does not matter
    uint64_t rflags = rflags_read();
    asm volatile("cli");

    cpu_t* cpu = smp_self_unsafe();
    if (cpu->cliAmount == 0)
    {
        cpu->prevFlags = rflags;
    }
    cpu->cliAmount++;
}

void cli_pop(void)
{
    if (!smp_initialized())
    {
        return;
    }

    cpu_t* cpu = smp_self_unsafe();

    LOG_ASSERT(cpu->cliAmount != 0, "cli amount underflow");

    cpu->cliAmount--;
    if (cpu->cliAmount == 0 && cpu->prevFlags & RFLAGS_INTERRUPT_ENABLE)
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

static void trap_begin(cpu_t* cpu)
{
    cpu->trapDepth++;
}

static void trap_end(cpu_t* cpu)
{
    cpu->trapDepth--;
}

void trap_handler(trap_frame_t* trapFrame)
{
    cpu_t* cpu = smp_self_unsafe();

    if (trapFrame->vector < IRQ_BASE)
    {
        exception_handler(trapFrame);
    }

    trap_begin(cpu);

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
        sched_schedule(trapFrame);
        lapic_eoi();
    }
    else if (trapFrame->vector == VECTOR_SCHED_INVOKE)
    {
        sched_schedule(trapFrame);
    }
    else
    {
        log_panic(trapFrame, "Unknown vector");
    }

    trap_end(cpu);
}
