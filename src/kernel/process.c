#include "process.h"

#include <string.h>

#include "heap.h"
#include "lock.h"
#include "vmm.h"
#include "pmm.h"
#include "gdt.h"
#include "regs.h"
#include "debug.h"

static _Atomic(pid_t) newPid = ATOMIC_VAR_INIT(0);

Process* process_new(const char *executable)
{
    Process *process = kmalloc(sizeof(Process));
    process->killed = false;
    process->id = atomic_fetch_add(&newPid, 1);
    memset(process->executable, 0, MAX_PATH);
    if (executable != NULL && vfs_realpath(process->executable, executable) == ERR)
    {
        memset(process->executable, 0, MAX_PATH);
    }
    vfs_context_init(&process->vfsContext);
    space_init(&process->space);
    atomic_init(&process->threadCount, 0);
    atomic_init(&process->newTid, 0);

    return process;
}

Thread* thread_new(Process* process, void* entry, uint8_t priority)
{
    atomic_fetch_add(&process->threadCount, 1);

    Thread* thread = kmalloc(sizeof(Thread));
    list_entry_init(&thread->base);
    thread->process = process;
    thread->id = atomic_fetch_add(&process->newTid, 1);
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->error = 0;
    thread->state = THREAD_STATE_ACTIVE;
    thread->priority = priority;
    thread->boost = 0;
    memset(&thread->kernelStack, 0, CONFIG_KERNEL_STACK);

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
        vfs_context_cleanup(&thread->process->vfsContext);
        space_cleanup(&thread->process->space);
        kfree(thread->process);
    }

    kfree(thread);
}