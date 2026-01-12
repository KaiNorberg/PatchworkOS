#include <kernel/cpu/interrupt.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/cpu/irq.h>
#include <kernel/cpu/regs.h>
#include <kernel/cpu/stack_pointer.h>
#include <kernel/drivers/perf.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/paging_types.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/wait.h>

#include <assert.h>

static void exception_handle_user(interrupt_frame_t* frame, const char* note)
{
    thread_t* thread = sched_thread_unsafe();
    if (thread_send_note(thread, note) == ERR)
    {
        atomic_store(&thread->state, THREAD_DYING);
        process_kill(thread->process, note);

        cpu_t* self = cpu_get();
        sched_do(frame, self);
    }
}

static uint64_t exception_grow_stack(thread_t* thread, uintptr_t faultAddr, stack_pointer_t* stack, pml_flags_t flags)
{
    uintptr_t alignedFaultAddr = ROUND_DOWN(faultAddr, PAGE_SIZE);
    if (stack_pointer_is_in_stack(stack, alignedFaultAddr, 1))
    {
        if (vmm_alloc(&thread->process->space, (void*)alignedFaultAddr, PAGE_SIZE, PAGE_SIZE, flags,
                VMM_ALLOC_FAIL_IF_MAPPED) == NULL)
        {
            if (errno == EEXIST) // Race condition, another CPU mapped the page.
            {
                return 0;
            }
            return ERR;
        }
        memset_s((void*)alignedFaultAddr, PAGE_SIZE, 0, PAGE_SIZE);

        return 1;
    }

    return 0;
}

static void exception_kernel_page_fault_handler(interrupt_frame_t* frame)
{
    thread_t* thread = sched_thread_unsafe();
    process_t* process = thread->process;
    uintptr_t faultAddr = (uintptr_t)cr2_read();

    if (frame->errorCode & PAGE_FAULT_PRESENT)
    {
        panic(frame, "page fault on present page at address 0x%llx", faultAddr);
    }

    uintptr_t alignedFaultAddr = ROUND_DOWN(faultAddr, PAGE_SIZE);
    if (stack_pointer_overlaps_guard(&thread->kernelStack, alignedFaultAddr, 1))
    {
        panic(frame, "kernel stack overflow at address 0x%llx", faultAddr);
    }

    uint64_t result = exception_grow_stack(thread, faultAddr, &thread->kernelStack, PML_WRITE | PML_PRESENT);
    if (result == ERR)
    {
        panic(frame, "failed to grow kernel stack for page fault at address 0x%llx", faultAddr);
    }

    if (result == 1)
    {
        return;
    }

    result = exception_grow_stack(thread, faultAddr, &thread->userStack, PML_USER | PML_WRITE | PML_PRESENT);
    if (result == ERR)
    {
        panic(frame, "failed to grow user stack for page fault at address 0x%llx", faultAddr);
    }

    if (result == 1)
    {
        return;
    }

    panic(frame, "invalid page fault at address 0x%llx", faultAddr);
}

static void exception_user_page_fault_handler(interrupt_frame_t* frame)
{
    thread_t* thread = sched_thread_unsafe();
    process_t* process = thread->process;
    uintptr_t faultAddr = (uintptr_t)cr2_read();

    if (frame->errorCode & PAGE_FAULT_PRESENT)
    {
        exception_handle_user(frame,
            F("pagefault at 0x%llx when %s present page at 0x%llx", frame->rip,
                (frame->errorCode & PAGE_FAULT_WRITE) ? "writing to" : "reading from", faultAddr));
        return;
    }

    uintptr_t alignedFaultAddr = ROUND_DOWN(faultAddr, PAGE_SIZE);
    if (stack_pointer_overlaps_guard(&thread->userStack, alignedFaultAddr, 1))
    {
        exception_handle_user(frame, F("pagefault at 0x%llx due to stack overflow at 0x%llx", frame->rip, faultAddr));
        return;
    }

    uint64_t result = exception_grow_stack(thread, faultAddr, &thread->userStack, PML_USER | PML_WRITE | PML_PRESENT);
    if (result == ERR)
    {
        exception_handle_user(frame, F("pagefault at 0x%llx failed to grow stack at 0x%llx", frame->rip, faultAddr));
        return;
    }

    if (result == 1)
    {
        return;
    }

    exception_handle_user(frame,
        F("pagefault at 0x%llx when %s 0x%llx", frame->rip,
            (frame->errorCode & PAGE_FAULT_WRITE) ? "writing" : "reading", faultAddr));
}

static void exception_handler(interrupt_frame_t* frame)
{
    errno_t err = errno;

    switch (frame->vector)
    {
    case VECTOR_DIVIDE_ERROR:
        if (!INTERRUPT_FRAME_IN_USER_SPACE(frame))
        {
            panic(frame, "divide by zero");
        }
        exception_handle_user(frame, F("divbyzero at 0x%llx", frame->rip));
        break;
    case VECTOR_INVALID_OPCODE:
        if (!INTERRUPT_FRAME_IN_USER_SPACE(frame))
        {
            panic(frame, "invalid opcode");
        }
        exception_handle_user(frame, F("illegal instruction at 0x%llx", frame->rip));
        break;
    case VECTOR_DOUBLE_FAULT:
        panic(frame, "double fault");
        break;
    case VECTOR_NMI:
        break; /// @todo Handle NMIs properly.
    case VECTOR_GENERAL_PROTECTION_FAULT:
        if (!INTERRUPT_FRAME_IN_USER_SPACE(frame))
        {
            panic(frame, "general protection fault");
        }
        exception_handle_user(frame, F("segfault at 0x%llx", frame->rip));
        break;
    case VECTOR_PAGE_FAULT:
        if (!INTERRUPT_FRAME_IN_USER_SPACE(frame))
        {
            exception_kernel_page_fault_handler(frame);
            return;
        }
        exception_user_page_fault_handler(frame);
        break;
    default:
        panic(frame, "unhandled exception vector 0x%x", frame->vector);
    }

    errno = err;
}

void interrupt_handler(interrupt_frame_t* frame)
{
    if (frame->vector < VECTOR_EXCEPTION_END) // Avoid extra stuff for exceptions
    {
        exception_handler(frame);
        return;
    }

    if (frame->vector == VECTOR_SPURIOUS)
    {
        return;
    }

    cpu_t* self = cpu_get();
    assert(self != NULL);

    self->inInterrupt = false;
    perf_interrupt_begin(self);

    percpu_update();

    if (frame->vector >= VECTOR_EXTERNAL_START && frame->vector < VECTOR_EXTERNAL_END)
    {
        irq_dispatch(frame, self);
    }
    else if (frame->vector == VECTOR_FAKE)
    {
        // Do nothing.
    }
    else if (frame->vector == VECTOR_TIMER)
    {
        timer_ack_eoi(frame, self);
    }
    else if (frame->vector == VECTOR_IPI)
    {
        ipi_handle_pending(frame, self);
    }
    else
    {
        panic(NULL, "Invalid interrupt vector 0x%x", frame->vector);
    }

    note_handle_pending(frame, self);
    wait_check_timeouts(frame, self);
    sched_do(frame, self);

    cpu_stacks_overflow_check(self);

    perf_interrupt_end(self);
    self->inInterrupt = false;

    // This is a sanity check to make sure blocking and scheduling is functioning correctly.
    assert(frame->rflags & RFLAGS_INTERRUPT_ENABLE);
}
