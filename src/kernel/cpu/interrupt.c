#include "interrupt.h"

#include "cpu.h"
#include "drivers/apic.h"
#include "drivers/statistics.h"
#include "gdt.h"
#include "interrupt.h"
#include "irq.h"
#include "log/panic.h"
#include "sched/thread.h"
#include "sched/wait.h"
#include "smp.h"

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
    switch (frame->vector)
    {
    case EXCEPTION_PAGE_FAULT:
        if (thread_handle_page_fault(frame) != ERR)
        {
            return;
        }
    default:
        break;
    }

    if (INTERRUPT_FRAME_IN_USER_SPACE(frame))
    {
        thread_t* thread = sched_thread_unsafe();
        process_t* process = thread->process;

        uint64_t cr2 = cr2_read();

        LOG_WARN(
            "unhandled user space exception in process pid=%d tid=%d vector=%lld error=0x%llx rip=0x%llx cr2=0x%llx\n",
            process->id, thread->id, frame->vector, frame->errorCode, frame->rip, cr2);

        sched_process_exit(EFAULT);
    }
    else
    {
        panic(frame, "unhandled kernel exception");
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
    case INTERRUPT_TLB_SHOOTDOWN:
    {
        vmm_shootdown_handler(frame, self);
        lapic_eoi();
    }
    break;
    case INTERRUPT_DIE:
    {
        sched_invoke(frame, self, SCHED_DIE);
    }
    break;
    case INTERRUPT_NOTE:
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
    case INTERRUPT_TIMER:
    {
        timer_interrupt_handler(frame, self);
        lapic_eoi();
    }
    break;
    case INTERRUPT_HALT:
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

    cpu_stacks_overflow_check(self);

    statistics_interrupt_end(frame, self);
    self->interrupt.inInterrupt = false;

    // This is a sanity check to make sure blocking and scheduling is functioning correctly. For instance, a trap should
    // never return with a lock acquired nor should one be invoked with a lock acquired.
    assert(frame->rflags & RFLAGS_INTERRUPT_ENABLE);
}
