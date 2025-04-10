#include "trap.h"

#include "apic.h"
#include "gdt.h"
#include "irq.h"
#include "loader.h"
#include "log.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "vectors.h"
#include "vmm.h"
#include "waitsys.h"

void cli_push(void)
{
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

void cli_pop(void)
{
    if (!smp_initialized())
    {
        return;
    }

    // Make sure interrupts are disabled
    asm volatile("cli");

    cpu_t* cpu = smp_self_unsafe();
    ASSERT_PANIC(cpu->cliAmount != 0, "cli amount underflow");
    cpu->cliAmount--;
    if (cpu->cliAmount == 0 && cpu->prevFlags & RFLAGS_INTERRUPT_ENABLE && cpu->trapDepth == 0)
    {
        asm volatile("sti");
    }
}

static void exception_handler(const trap_frame_t* trapFrame)
{
    if (trapFrame->ss == GDT_USER_DATA && trapFrame->cs == GDT_USER_CODE)
    {
        log_panic(trapFrame, "Unhandled User Exception");
    }
    else
    {
        // TODO: Add handling for user exceptions
        log_panic(trapFrame, "Exception");
        // sched_process_exit(1);
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

void trap_handler(trap_frame_t* trapFrame)
{
    if (trapFrame->vector < VECTOR_IRQ_BASE)
    {
        exception_handler(trapFrame);
    }

    cpu_t* cpu = smp_self_unsafe();
    cpu->trapDepth++;

    if (trapFrame->vector >= VECTOR_IRQ_BASE && trapFrame->vector < VECTOR_IRQ_BASE + IRQ_AMOUNT)
    {
        irq_dispatch(trapFrame);
    }
    else if (trapFrame->vector == VECTOR_IPI)
    {
        ipi_handler(trapFrame);
    }
    else if (trapFrame->vector == VECTOR_TIMER)
    {
        waitsys_timer_trap(trapFrame);
        sched_timer_trap(trapFrame);
        lapic_eoi();
    }
    else if (trapFrame->vector == VECTOR_SCHED_SCHEDULE)
    {
        sched_schedule_trap(trapFrame);
    }
    else if (trapFrame->vector == VECTOR_WAITSYS_BLOCK)
    {
        waitsys_block_trap(trapFrame);
    }
    else
    {
        log_panic(trapFrame, "Unknown vector");
    }

    cpu->trapDepth--;
}
