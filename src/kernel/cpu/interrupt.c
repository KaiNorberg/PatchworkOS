#include <kernel/cpu/interrupt.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/cpu/irq.h>
#include <kernel/drivers/perf.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/wait.h>

#include <kernel/cpu/regs.h>

#include <assert.h>

void interrupt_ctx_init(interrupt_ctx_t* ctx)
{
    ctx->oldRflags = 0;
    ctx->disableDepth = 0;
}

void interrupt_disable(void)
{
    uint64_t rflags = rflags_read();
    asm volatile("cli" ::: "memory");
    interrupt_ctx_t* ctx = &cpu_get_unsafe()->interrupt;
    if (ctx->disableDepth == 0)
    {
        ctx->oldRflags = rflags;
    }
    ctx->disableDepth++;
}

void interrupt_enable(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    interrupt_ctx_t* ctx = &cpu_get_unsafe()->interrupt;
    assert(ctx->disableDepth != 0);
    ctx->disableDepth--;

    if (ctx->disableDepth == 0 && (ctx->oldRflags & RFLAGS_INTERRUPT_ENABLE))
    {
        asm volatile("sti" ::: "memory");
    }
}

uint64_t page_fault_handler(const interrupt_frame_t* frame)
{
    thread_t* thread = sched_thread_unsafe();
    if (thread == NULL)
    {
        return ERR;
    }
    uintptr_t faultAddr = (uintptr_t)cr2_read();

    if (frame->errorCode & PAGE_FAULT_PRESENT)
    {
        errno = EFAULT;
        return ERR;
    }

    uintptr_t alignedFaultAddr = ROUND_DOWN(faultAddr, PAGE_SIZE);
    if (stack_pointer_is_in_stack(&thread->userStack, alignedFaultAddr, 1))
    {
        if (vmm_alloc(&thread->process->space, (void*)alignedFaultAddr, PAGE_SIZE, PML_WRITE | PML_PRESENT | PML_USER,
                VMM_ALLOC_FAIL_IF_MAPPED) == NULL)
        {
            if (errno == EEXIST) // Race condition, another CPU mapped the page.
            {
                return 0;
            }

            return ERR;
        }
        memset((void*)alignedFaultAddr, 0, PAGE_SIZE);
        return 0;
    }

    if (INTERRUPT_FRAME_IN_USER_SPACE(frame))
    {
        errno = EFAULT;
        return ERR;
    }

    if (stack_pointer_is_in_stack(&thread->kernelStack, alignedFaultAddr, 1))
    {
        if (vmm_alloc(&thread->process->space, (void*)alignedFaultAddr, PAGE_SIZE, PML_WRITE | PML_PRESENT,
                VMM_ALLOC_FAIL_IF_MAPPED) == NULL)
        {
            if (errno == EEXIST) // Race condition, another CPU mapped the page.
            {
                return 0;
            }
            return ERR;
        }
        memset((void*)alignedFaultAddr, 0, PAGE_SIZE);
        return 0;
    }

    errno = EFAULT;
    return ERR;
}

static void exception_handler(interrupt_frame_t* frame)
{
    switch (frame->vector)
    {
    case VECTOR_DOUBLE_FAULT:
        panic(frame, "kernel double fault");
        break;
    case VECTOR_NMI:
        return; // TODO: Handle NMIs properly.
    case VECTOR_PAGE_FAULT:
        if (page_fault_handler(frame) != ERR)
        {
            return;
        }
        break;
    default:
        break;
    }

    if (INTERRUPT_FRAME_IN_USER_SPACE(frame))
    {
        cpu_t* self = cpu_get_unsafe();
        thread_t* thread = sched_thread_unsafe();
        process_t* process = thread->process;

        uint64_t cr2 = cr2_read();

        LOG_WARN("unhandled user space exception in process pid=%d tid=%d vector=%lld error=0x%llx rip=0x%llx "
                 "cr2=0x%llx errno='%s'\n",
            process->id, thread->id, frame->vector, frame->errorCode, frame->rip, cr2, strerror(thread->error));

#ifndef NDEBUG
        panic_stack_trace(frame);
#endif

        process_kill(process, EFAULT);
        sched_do(frame, self);
    }
    else
    {
        panic(frame, "unhandled kernel exception");
    }
}

void interrupt_handler(interrupt_frame_t* frame)
{
    if (frame->vector < VECTOR_EXCEPTION_END) // Avoid extra stuff for exceptions
    {
        exception_handler(frame);
        return;
    }

    cpu_t* self = cpu_get_unsafe();
    assert(self != NULL);

    perf_interrupt_begin(self);

    if (frame->vector >= VECTOR_EXTERNAL_START && frame->vector < VECTOR_EXTERNAL_END)
    {
        irq_dispatch(frame, self);
    }
    else
    {
        switch (frame->vector)
        {
        case VECTOR_TIMER:
            timer_dispatch(frame, self);
            break;
        case VECTOR_IPI:
            ipi_dispatch(frame, self);
            break;
        case VECTOR_SPURIOUS:
            LOG_DEBUG("spurious interrupt on cpu id=%u\n", self->id);
            break;
        default:
            panic(NULL, "invalid internal interrupt vector 0x%x", frame->vector);
        }
    }

    note_handle_pending(frame, self);
    sched_do(frame, self);

    cpu_stacks_overflow_check(self);

    perf_interrupt_end(self);

    // This is a sanity check to make sure blocking and scheduling is functioning correctly. For instance, a trap should
    // never return with a lock acquired nor should one be invoked with a lock acquired.
    assert(frame->rflags & RFLAGS_INTERRUPT_ENABLE);
}
