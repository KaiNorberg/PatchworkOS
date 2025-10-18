#include "interrupt.h"

#include "cpu.h"
#include "drivers/apic.h"
#include "gdt.h"
#include "irq.h"
#include "log/panic.h"
#include "sched/thread.h"
#include "sched/wait.h"
#include "smp.h"
#include "utils/statistics.h"
#include "vectors.h"

#include <common/regs.h>

#include <assert.h>

void interrupt_ctx_init(interrupt_ctx_t* ctx)
{
    ctx->oldRflags = 0;
    ctx->disableDepth = 0;
    ctx->inInterrupt = false;
}

void interrupt_disable(void)
{
    uint64_t rflags = rflags_read();
    asm volatile("cli");
    interrupt_ctx_t* ctx = &smp_self_unsafe()->interrupt;
    if (ctx->disableDepth == 0)
    {
        ctx->oldRflags = rflags;
    }
    ctx->disableDepth++;
}

void interrupt_enable(void)
{
    uint64_t rflags = rflags_read();
    assert(!(rflags & RFLAGS_INTERRUPT_ENABLE));

    interrupt_ctx_t* ctx = &smp_self_unsafe()->interrupt;
    assert(ctx->disableDepth != 0);
    ctx->disableDepth--;

    if (ctx->disableDepth == 0 && (ctx->oldRflags & RFLAGS_INTERRUPT_ENABLE))
    {
        asm volatile("sti");
    }
}

static void exception_handler(interrupt_frame_t* frame)
{
    if (frame->vector == EXCEPTION_PAGE_FAULT)
    {
        if (frame->errorCode & PAGE_FAULT_PRESENT)
        {
            panic(frame, "Page fault caused by present page");
        }

        if (thread_handle_page_fault(frame) == ERR)
        {
            panic(frame, "Page fault could not be handled");
        }
    }
    else
    {
        panic(frame, "Exception");
    }
}

void interrupt_handler(interrupt_frame_t* frame)
{
    if (frame->vector < EXCEPTION_AMOUNT)
    {
        exception_handler(frame);
        return;
    }

    cpu_t* self = smp_self_unsafe();

    if (self->interrupt.inInterrupt)
    {
        panic(frame, "Interrupt handler called while already in an interrupt");
    }
    self->interrupt.inInterrupt = true;

    statistics_interrupt_begin(frame, self);

    switch (frame->vector)
    {
    case VECTOR_NOTE:
    {
        // Notes should only be handled when in user space otherwise we get so many edge cases to deal with.
        // There is a check when a system call occurs to make sure that the note will eventually be handled.
        if (INTERRUPT_FRAME_IN_USER_SPACE(frame))
        {
            note_dispatch(frame, self);
        }
        lapic_eoi();
    }
    break;
    case VECTOR_TIMER:
    {
        timer_interrupt_handler(frame, self);
        lapic_eoi();
    }
    break;
    case VECTOR_HALT:
    {
        while (1)
        {
            asm volatile("hlt");
        }
    }
    break;
    default:
    {
        if (frame->vector < EXTERNAL_INTERRUPT_BASE + IRQ_AMOUNT)
        {
            irq_dispatch(frame);
        }
        else
        {
            panic(frame, "Unknown vector");
        }
    }
    }

    statistics_interrupt_end(frame, self);
    self->interrupt.inInterrupt = false;

    // This is a sanity check to make sure blocking and scheduling is functioning correctly. For instance, a trap should
    // never return with a lock acquired nor should one be invoked with a lock acquired.
    assert(frame->rflags & RFLAGS_INTERRUPT_ENABLE);
}
