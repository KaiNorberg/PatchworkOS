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

#include <assert.h>
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
    assert(!(rflags & RFLAGS_INTERRUPT_ENABLE));

    cli_ctx_t* cli = &smp_self_unsafe()->cli;
    assert(cli->depth != 0);
    cli->depth--;

    if (cli->depth == 0 && cli->intEnable)
    {
        asm volatile("sti");
    }
}

static void exception_handler(trap_frame_t* trapFrame)
{
    if (TRAP_FRAME_IN_USER_SPACE(trapFrame))
    {
        thread_t* thread = sched_thread();
        if (thread == NULL)
        {
            log_panic(trapFrame, "Unhandled User Exception");
        }

        printf("user exception: process killed due to exception tid=%d pid=%d vector=0x%x error=%p rip=%p cr2=%p\n",
            thread->id, thread->process->id, trapFrame->vector, trapFrame->errorCode, trapFrame->rip, cr2_read());

        sched_process_exit(0);
        sched_schedule(trapFrame, smp_self_unsafe());
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

    switch (trapFrame->vector)
    {
    case VECTOR_SCHED_INVOKE:
    {
        // Does nothing
    }
    break;
    case VECTOR_IPI:
    {
        smp_ipi_receive(trapFrame, self);
        lapic_eoi();
    }
    break;
    case VECTOR_TIMER:
    {
        wait_timer_trap(trapFrame, self);
        sched_timer_trap(trapFrame, self);
        lapic_eoi();
    }
    break;
    case VECTOR_WAIT_BLOCK:
    {
        wait_block_trap(trapFrame, self);
    }
    break;
    default:
    {
        if (trapFrame->vector < VECTOR_IRQ_BASE + IRQ_AMOUNT)
        {
            irq_dispatch(trapFrame);
        }
        else
        {
            log_panic(trapFrame, "Unknown vector");
        }
    }
    }

    sched_schedule(trapFrame, self);

    if (TRAP_FRAME_IN_USER_SPACE(trapFrame))
    {
        note_dispatch(trapFrame, self);
    }

    statistics_trap_end(trapFrame, self);
    self->trapDepth--;

    // This is a sanity check to make sure blocking and scheduling is functioning correctly. For instance, a trap should
    // never return with a lock acquired nor should one be called with a lock acquired.
    assert(trapFrame->rflags & RFLAGS_INTERRUPT_ENABLE);
}
