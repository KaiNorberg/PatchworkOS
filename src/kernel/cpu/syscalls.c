#include "syscalls.h"

#include "cpu/gdt.h"
#include "cpu/regs.h"
#include "gdt.h"
#include "sched/sched.h"
#include "sched/thread.h"

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
    LOG_INFO("sorting syscall table\n");

    // Syscalls are not inserted into the table by the linker in the correct order so we sort them.
    const uint64_t syscallsInTable =
        (((uint64_t)_syscallTableEnd - (uint64_t)_syscallTableStart) / sizeof(syscall_descriptor_t));
    assert(syscallsInTable == SYS_TOTAL_AMOUNT);

    qsort(_syscallTableStart, syscallsInTable, sizeof(syscall_descriptor_t), syscall_descriptor_cmp);

    for (uint64_t i = 0; i < syscallsInTable; i++)
    {
        assert(_syscallTableStart[i].number == i);
    }
}

void syscalls_cpu_init(void)
{
    // Read this to understand whats happening https://www.felixcloutier.com/x86/syscall,
    // https://www.felixcloutier.com/x86/sysret.

    msr_write(MSR_EFER, msr_read(MSR_EFER) | EFER_SYSCALL_ENABLE);

    msr_write(MSR_STAR, ((uint64_t)(GDT_USER_CODE - 16) | GDT_RING3) << 48 | ((uint64_t)(GDT_KERNEL_CODE)) << 32);
    msr_write(MSR_LSTAR, (uint64_t)syscall_entry);

    msr_write(MSR_SYSCALL_FLAG_MASK,
        RFLAGS_TRAP | RFLAGS_DIRECTION | RFLAGS_INTERRUPT_ENABLE | RFLAGS_IOPL | RFLAGS_AUX_CARRY | RFLAGS_NESTED_TASK);
}

void syscall_ctx_init(syscall_ctx_t* ctx, uint64_t kernelRsp)
{
    ctx->kernelRsp = kernelRsp;
    ctx->userRsp = 0;
    ctx->inSyscall = false;
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

void syscall_handler(trap_frame_t* trapFrame)
{
    uint64_t number = trapFrame->rax;

    const syscall_descriptor_t* desc = syscall_get_descriptor(number);
    if (desc == NULL)
    {
        LOG_DEBUG("Unknown syscall %u\n", number);
        errno = ENOSYS;
        trapFrame->rax = ERR;
        return;
    }

    thread_t* thread = sched_thread();
    thread->syscall.inSyscall = true;

    uint64_t args[6] = {trapFrame->rdi, trapFrame->rsi, trapFrame->rdx, trapFrame->r10, trapFrame->r8, trapFrame->r9};

    uint64_t (*handler)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) = desc->handler;
    trapFrame->rax = handler(args[0], args[1], args[2], args[3], args[4], args[5]);

    thread->syscall.inSyscall = false;
    note_dispatch_invoke();
}

bool syscall_is_pointer_valid(const void* pointer, uint64_t length)
{
    if (length == 0)
    {
        return true;
    }

    if (pointer == NULL)
    {
        return false;
    }

    uintptr_t start = (uintptr_t)pointer;
    uintptr_t end = (uintptr_t)pointer + length;

    if (start > end) // Overflow
    {
        return false;
    }

    if (end > PML_LOWER_HALF_END || (uintptr_t)start < PML_LOWER_HALF_START)
    {
        return false;
    }

    return true;
}

bool syscall_is_buffer_valid(space_t* space, const void* pointer, uint64_t length)
{
    if (length == 0)
    {
        return true;
    }

    if (!syscall_is_pointer_valid(pointer, length))
    {
        return false;
    }

    if (!vmm_mapped(space, pointer, length))
    {
        return false;
    }

    return true;
}

bool syscall_is_string_valid(space_t* space, const char* string)
{
    if (!syscall_is_buffer_valid(space, string, sizeof(const char*)))
    {
        return false;
    }

    const char* chr = string;
    while (true)
    {
        if (!syscall_is_buffer_valid(space, chr, sizeof(char)))
        {
            return false;
        }

        if (*chr == '\0')
        {
            return true;
        }

        chr++;
    }
}
