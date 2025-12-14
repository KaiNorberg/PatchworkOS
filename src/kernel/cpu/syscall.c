#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/syscall.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/log/log.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>

#include <kernel/defs.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

void syscall_ctx_init(syscall_ctx_t* ctx, const stack_pointer_t* syscallStack)
{
    ctx->syscallRsp = syscallStack->top;
    ctx->userRsp = 0;
    ctx->frame = NULL;
    ctx->flags = SYSCALL_NORMAL;
}

void syscall_ctx_load(syscall_ctx_t* ctx)
{
    msr_write(MSR_KERNEL_GS_BASE, (uint64_t)ctx);
}

static int syscall_descriptor_cmp(const void* a, const void* b)
{
    const syscall_descriptor_t* sysA = (const syscall_descriptor_t*)a;
    const syscall_descriptor_t* sysB = (const syscall_descriptor_t*)b;
    return sysA->number - sysB->number;
}

void syscall_table_init(void)
{
    // Syscalls are not inserted into the table by the linker in the correct order so we sort them.
    const uint64_t syscallsInTable =
        (((uint64_t)_syscallTableEnd - (uint64_t)_syscallTableStart) / sizeof(syscall_descriptor_t));
    assert(syscallsInTable == SYS_TOTAL_AMOUNT);

    LOG_INFO("sorting syscall table, total system calls %d\n", SYS_TOTAL_AMOUNT);
    qsort(_syscallTableStart, syscallsInTable, sizeof(syscall_descriptor_t), syscall_descriptor_cmp);

    for (uint64_t i = 0; i < syscallsInTable; i++)
    {
        assert(_syscallTableStart[i].number == i);
    }
}

void syscalls_cpu_init(void)
{
    msr_write(MSR_EFER, msr_read(MSR_EFER) | EFER_SYSCALL_ENABLE);

    msr_write(MSR_STAR, ((uint64_t)(GDT_USER_CODE - 16) | GDT_RING3) << 48 | ((uint64_t)(GDT_KERNEL_CODE)) << 32);
    msr_write(MSR_LSTAR, (uint64_t)syscall_entry);

    msr_write(MSR_SYSCALL_FLAG_MASK,
        RFLAGS_TRAP | RFLAGS_DIRECTION | RFLAGS_INTERRUPT_ENABLE | RFLAGS_IOPL | RFLAGS_AUX_CARRY | RFLAGS_NESTED_TASK);
}

const syscall_descriptor_t* syscall_get_descriptor(uint64_t number)
{
    if (number > SYS_TOTAL_AMOUNT)
    {
        return NULL;
    }

    return &_syscallTableStart[number];
}

void syscall_handler(interrupt_frame_t* frame)
{
    const syscall_descriptor_t* desc = syscall_get_descriptor(frame->rax);
    if (desc == NULL)
    {
        LOG_DEBUG("Unknown syscall %u\n", frame->rax);
        errno = ENOSYS;
        frame->rax = ERR;
        return;
    }

    thread_t* thread = sched_thread();
    perf_syscall_begin();

    thread->syscall.frame = frame;
    thread->syscall.flags = SYSCALL_NORMAL;

    // This is safe for any input type and any number of arguments up to 6 as they will simply be ignored.
    frame->rax = desc->handler(frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);

    perf_syscall_end();

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    asm volatile("cli" ::: "memory");
    if (thread_is_note_pending(thread) || (thread->syscall.flags & SYSCALL_FORCE_FAKE_INTERRUPT))
    {
        cpu_t* self = cpu_get_unsafe();
        frame->vector = VECTOR_FAKE;
        interrupt_fake(frame, self);
    }
}
