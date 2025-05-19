#include "trap.h"

#include "apic.h"
#include "gdt.h"
#include "irq.h"
#include "mem/vmm.h"
#include "regs.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "smp.h"
#include "utils/log.h"
#include "utils/statistics.h"
#include "vectors.h"

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
        thread_t* thread = sched_thread();
        if (thread == NULL)
        {
            log_panic(trapFrame, "Unhandled User Exception");
        }

        printf("user exception: process killed due to exception tid=%d pid=%d vector=0x%x error=%p rip=%p cr2=%p\n", thread->id, thread->process->id,
            trapFrame->vector, trapFrame->errorCode, trapFrame->rip, cr2_read());

        sched_process_exit(0);
        sched_schedule_trap(trapFrame, smp_self_unsafe());
    }
    else
    {
        log_panic(trapFrame, "Exception");
    }
}

void trap_handler(trap_frame_t* trapFrame)
{
    if (trapFrame->vector < VECTOR_IRQ_BASE)
    {
        exception_handler(trapFrame);
        return;
    }

    cpu_t* self = smp_self_unsafe();

    self->trapDepth++;
    statistics_trap_begin(trapFrame, self);

    if (trapFrame->vector >= VECTOR_IRQ_BASE && trapFrame->vector < VECTOR_IRQ_BASE + IRQ_AMOUNT)
    {
        irq_dispatch(trapFrame);
    }
    else if (trapFrame->vector == VECTOR_IPI)
    {
        smp_ipi_recieve(trapFrame, self);
        lapic_eoi();
    }
    else if (trapFrame->vector == VECTOR_TIMER)
    {
        wait_timer_trap(trapFrame, self);
        sched_timer_trap(trapFrame, self);
        lapic_eoi();
    }
    else if (trapFrame->vector == VECTOR_SCHED_SCHEDULE)
    {
        sched_schedule_trap(trapFrame, self);
    }
    else if (trapFrame->vector == VECTOR_WAIT_BLOCK)
    {
        wait_block_trap(trapFrame, self);
    }
    else
    {
        log_panic(trapFrame, "Unknown vector");
    }

    note_trap_handler(trapFrame, self);

    // This is a sanity check to make sure blocking and scheduling is functioning correctly. For instance, a trap should
    // never return with a lock acquired.
    if (!(trapFrame->rflags & RFLAGS_INTERRUPT_ENABLE))
    {
        log_panic(trapFrame, "Returning to frame with interrupts disabled");
    }

    statistics_trap_end(trapFrame, self);
    self->trapDepth--;
}
