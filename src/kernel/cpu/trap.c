#include "trap.h"

#include "drivers/apic.h"
#include "irq.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "smp.h"
#include "utils/statistics.h"
#include "vectors.h"

#include <common/regs.h>

#include <assert.h>

void cli_ctx_init(cli_ctx_t* cli)
{
    cli->oldRflags = 0;
    cli->depth = 0;
}

void cli_push(void)
{
    uint64_t rflags = rflags_read();
    asm volatile("cli");
    cli_ctx_t* cli = &smp_self_unsafe()->cli;
    if (cli->depth == 0)
    {
        cli->oldRflags = rflags;
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

    if (cli->depth == 0 && (cli->oldRflags & RFLAGS_INTERRUPT_ENABLE))
    {
        asm volatile("sti");
    }
}

static void exception_handler(trap_frame_t* trapFrame)
{
    if (trapFrame->vector == EXCEPTION_PAGE_FAULT)
    {
        if (thread_handle_page_fault(trapFrame) == ERR)
        {
            panic(trapFrame, "Page fault could not be handled");
        }
    }
    else
    {
        panic(trapFrame, "Exception");
    }
}

void trap_handler(trap_frame_t* trapFrame)
{
    if (trapFrame->vector < EXCEPTION_AMOUNT)
    {
        exception_handler(trapFrame);
        return;
    }

    cpu_t* self = smp_self_unsafe();

    self->trapDepth++;
    if (self->trapDepth != 1)
    {
        panic(trapFrame, "self->trapDepth != 1");
    }

    statistics_trap_begin(trapFrame, self);

    switch (trapFrame->vector)
    {
    case VECTOR_HALT:
    {
        while (1)
        {
            asm volatile("hlt");
        }
    }
    break;
    case VECTOR_TIMER:
    {
        timer_trap_handler(trapFrame, self);
        lapic_eoi();
    }
    break;
    default:
    {
        if (trapFrame->vector < EXTERNAL_INTERRUPT_BASE + IRQ_AMOUNT)
        {
            irq_dispatch(trapFrame);
        }
        else
        {
            panic(trapFrame, "Unknown vector");
        }
    }
    }

    // TODO: Consider removing this?
    sched_schedule(trapFrame, self);

    if (!self->sched.runThread->syscall.inSyscall)
    {
        note_dispatch(trapFrame, self);
    }

    statistics_trap_end(trapFrame, self);
    self->trapDepth--;

    // This is a sanity check to make sure blocking and scheduling is functioning correctly. For instance, a trap should
    // never return with a lock acquired nor should one be invoked with a lock acquired.
    assert(trapFrame->rflags & RFLAGS_INTERRUPT_ENABLE);
}
