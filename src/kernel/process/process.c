#include "process.h"

#include <stdatomic.h>
#include <string.h>

#include "heap/heap.h"
#include "lock/lock.h"
#include "vmm/vmm.h"
#include "pmm/pmm.h"
#include "gdt/gdt.h"
#include "regs/regs.h"
#include "debug/debug.h"

static _Atomic uint64_t newPid = 0;

Process* process_new(const char* executable)
{
    Process* process = kmalloc(sizeof(Process));
    process->id = atomic_fetch_add(&newPid, 1);
    if (executable != 0)
    {
        strcpy(process->executable, executable);
    }
    else
    {
        memset(process->executable, 0, CONFIG_MAX_PATH);
    }
    file_table_init(&process->fileTable);
    space_init(&process->space);
    process->killed = false;
    process->threadCount = 0;
    process->newTid = 0;

    return process;
}

Thread* thread_new(Process* process, void* entry, uint8_t priority)
{
    atomic_fetch_add(&process->threadCount, 1);

    Thread* thread = kmalloc(sizeof(Thread));
    thread->process = process;
    thread->id = atomic_fetch_add(&process->newTid, 1);
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->error = 0;
    thread->state = THREAD_STATE_ACTIVE;
    thread->priority = priority;
    thread->boost = 0;

    memset(&thread->trapFrame, 0, sizeof(TrapFrame));
    thread->trapFrame.rip = (uint64_t)entry;
    thread->trapFrame.rsp = ((uint64_t)thread->kernelStack) + CONFIG_KERNEL_STACK;
    thread->trapFrame.cs = GDT_KERNEL_CODE;
    thread->trapFrame.ss = GDT_KERNEL_DATA;
    thread->trapFrame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    return thread;
}

void thread_free(Thread* thread)
{
    if (atomic_fetch_sub(&thread->process->threadCount, 1) <= 1)
    {
        file_table_cleanup(&thread->process->fileTable);
        space_cleanup(&thread->process->space);
        kfree(thread->process);
    }

    kfree(thread);
}