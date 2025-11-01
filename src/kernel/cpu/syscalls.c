#include <kernel/cpu/syscalls.h>

#include <kernel/cpu/gdt.h>
#include <kernel/cpu/smp.h>
#include <kernel/drivers/apic.h>
#include <kernel/log/log.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>

#include <kernel/defs.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

int syscall_descriptor_cmp(const void* a, const void* b)
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

void syscall_ctx_init(syscall_ctx_t* ctx, stack_pointer_t* kernelStack)
{
    ctx->kernelRsp = kernelStack->top;
    ctx->userRsp = 0;
}

void syscall_ctx_load(syscall_ctx_t* ctx)
{
    msr_write(MSR_GS_BASE, (uint64_t)ctx);
    msr_write(MSR_KERNEL_GS_BASE, (uint64_t)ctx);
}

const syscall_descriptor_t* syscall_get_descriptor(uint64_t number)
{
    if (number > SYS_TOTAL_AMOUNT)
    {
        return NULL;
    }

    return &_syscallTableStart[number];
}

uint64_t syscall_handler(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t rcx, uint64_t r8, uint64_t r9,
    uint64_t number)
{
    const syscall_descriptor_t* desc = syscall_get_descriptor(number);
    if (desc == NULL)
    {
        LOG_DEBUG("Unknown syscall %u\n", number);
        errno = ENOSYS;
        return ERR;
    }

    perf_update(smp_self_unsafe(), PERF_SWITCH_ENTER_SYSCALL);
    asm volatile("sti");

    // This is safe for any input type and any number of arguments up to 6 as they will simply be ignored.
    uint64_t (*handler)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) = desc->handler;
    uint64_t result = handler(rdi, rsi, rdx, rcx, r8, r9);

    // Interrupts will be renabled when the sysret instruction executes, this means that if there is a pending note and
    // we invoke an interrupt the actual interrupt handler will not run until we return to user space.
    asm volatile("cli");
    perf_update(smp_self_unsafe(), PERF_SWITCH_LEAVE_SYSCALL);

    if (note_queue_length(&sched_thread_unsafe()->notes) > 0)
    {
        lapic_send_ipi(lapic_self_id(), INTERRUPT_NOTE);
    }
    return result;
}
