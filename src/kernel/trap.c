#include "trap.h"

#include "apic.h"
#include "gdt.h"
#include "irq.h"
#include "loader.h"
#include "log.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "statistics.h"
#include "vectors.h"
#include "vmm.h"
#include "wait.h"

#include <stdio.h>

void cli_ctx_init(cli_ctx_t* cli)
{
    cli->intEnable = false;
    cli->depth = 0;
}

void cli_push(void)
{
    uint64_t rflags = rflags_read();
    asm volatile("cli");
    cli_ctx_t* cli = &smp_self_unsafe()->cli;
    if (cli->depth == 0)
    {
        cli->intEnable = rflags & RFLAGS_INTERRUPT_ENABLE;
    }
    cli->depth++;
}

void cli_pop(void)
{
    uint64_t rflags = rflags_read();
    ASSERT_PANIC(!(rflags & RFLAGS_INTERRUPT_ENABLE));

    cli_ctx_t* cli = &smp_self_unsafe()->cli;
    ASSERT_PANIC(cli->depth != 0);
    cli->depth--;

    if (cli->depth == 0 && cli->intEnable)
    {
        asm volatile("sti");
    }
}

static void exception_handler(trap_frame_t* trapFrame)
{
    if (TRAP_FRAME_FROM_USER_SPACE(trapFrame))
    {
        process_t* process = sched_process();
        if (process == NULL)
        {
            log_panic(trapFrame, "Unhandled User Exception");
        }

        printf("user exception: process killed due to exception pid=%d vector=0x%x error=%b rip=%p", process->id,
            trapFrame->vector, trapFrame->errorCode, trapFrame->rip);
        atomic_store(&process->dead, true);
        sched_schedule_trap(trapFrame);
    }
    else
    {
        log_panic(trapFrame, "Exception");
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
        return;
    }

    cpu_t* cpu = smp_self_unsafe();
    cpu->trapDepth++;

    statistics_trap_begin(trapFrame, cpu);

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
        wait_timer_trap(trapFrame);
        sched_timer_trap(trapFrame);
        lapic_eoi();
    }
    else if (trapFrame->vector == VECTOR_SCHED_SCHEDULE)
    {
        sched_schedule_trap(trapFrame);
    }
    else if (trapFrame->vector == VECTOR_WAIT_BLOCK)
    {
        wait_block_trap(trapFrame);
    }
    else
    {
        log_panic(trapFrame, "Unknown vector");
    }

    statistics_trap_end(trapFrame, cpu);

    // This is a sanity check to make sure blocking and scheduling is functioning correctly. For instance, a trap should
    // never return with a lock acquired.
    if (!(trapFrame->rflags & RFLAGS_INTERRUPT_ENABLE))
    {
        log_panic(trapFrame, "Returning to frame with interrupts disabled");
    }
    cpu->trapDepth--;
}
