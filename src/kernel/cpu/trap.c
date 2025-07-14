#include "trap.h"

#include "apic.h"
#include "gdt.h"
#include "irq.h"
#include "kernel.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "regs.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "smp.h"
#include "utils/statistics.h"
#include "vectors.h"

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
    if (TRAP_FRAME_IN_USER_SPACE(trapFrame))
    {
        thread_t* thread = sched_thread();
        if (thread == NULL)
        {
            panic(trapFrame, "User exception on NULL thread");
        }

        switch (trapFrame->vector)
        {
        case VECTOR_PAGE_FAULT:
        {
            uintptr_t faultAddress = cr2_read();
            if (faultAddress >= LOADER_GUARD_PAGE_BOTTOM(thread->id) &&
                faultAddress <= LOADER_GUARD_PAGE_TOP(thread->id)) // Fault in guard page
            {
                LOG_WARN("process killed due to stack overflow tid=%d pid=%d address=%p rip=%p\n", thread->id,
                    thread->process->id, faultAddress, trapFrame->rip);

                break;
            }
            else if (faultAddress >= LOADER_USER_STACK_BOTTOM(thread->id) &&
                faultAddress <= LOADER_USER_STACK_TOP(thread->id) &&
                !(trapFrame->vector & PAGE_FAULT_PRESENT)) // Fault in user stack region due to non present page
            {
                uintptr_t pageAddress = ROUND_DOWN(faultAddress, PAGE_SIZE);
                LOG_DEBUG("expanding user stack %p pid=%d tid=%d\n", pageAddress, thread->process->id, thread->id);
                if (vmm_alloc(&thread->process->space, (void*)pageAddress, PAGE_SIZE, PROT_READ | PROT_WRITE) != NULL)
                {
                    return;
                }
            }

            LOG_WARN("process killed due to page fault tid=%d pid=%d address=%p rip=%p error=0x%x\n", thread->id,
                thread->process->id, faultAddress, trapFrame->rip, trapFrame->errorCode);
        }
        break;
        default:
        {
            LOG_WARN("process killed due to exception tid=%d pid=%d vector=0x%x error=%p rip=%p cr2=%p\n", thread->id,
                thread->process->id, trapFrame->vector, trapFrame->errorCode, trapFrame->rip, cr2_read());
        }
        }

        sched_process_exit(0);
        sched_schedule(trapFrame, smp_self_unsafe());
    }
    else
    {
        panic(trapFrame, "Exception");
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
    case VECTOR_NOTIFY:
    {
        lapic_eoi();
    }
    break;
    case VECTOR_TIMER:
    {
        systime_timer_trap(trapFrame, self);
        wait_timer_trap(trapFrame, self);
        lapic_eoi();
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
            panic(trapFrame, "Unknown vector");
        }
    }
    }

    sched_schedule(trapFrame, self);

    if (!self->sched.runThread->syscall.inSyscall)
    {
        note_dispatch(trapFrame, self);
    }

    if (self->sched.runThread->canary != THREAD_CANARY)
    {
        panic(trapFrame, "self->sched.runThread->canary != THREAD_CANARY");
    }

    statistics_trap_end(trapFrame, self);
    self->trapDepth--;

    // This is a sanity check to make sure blocking and scheduling is functioning correctly. For instance, a trap should
    // never return with a lock acquired nor should one be invoked with a lock acquired.
    assert(trapFrame->rflags & RFLAGS_INTERRUPT_ENABLE);
}
