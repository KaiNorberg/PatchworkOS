#include "process.h"

#include <stdatomic.h>
#include <string.h>

#include "heap/heap.h"
#include "lock/lock.h"
#include "vmm/vmm.h"
#include "pmm/pmm.h"
#include "gdt/gdt.h"
#include "registers/registers.h"
#include "debug/debug.h"

static _Atomic uint64_t newPid = 0;

Process* process_new(void)
{
    Process* process = kmalloc(sizeof(Process));
    process->id = atomic_fetch_add(&newPid, 1);
    file_table_init(&process->fileTable);
    process->addressSpace = address_space_new();
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
    thread->errno = 0;
    thread->state = THREAD_STATE_ACTIVE;
    thread->priority = priority;
    thread->boost = 0;

    memset(&thread->interruptFrame, 0, sizeof(InterruptFrame));
    thread->interruptFrame.instructionPointer = (uint64_t)entry;
    thread->interruptFrame.stackPointer = (uint64_t)VMM_LOWER_HALF_MAX;
    thread->interruptFrame.codeSegment = GDT_USER_CODE | 3;
    thread->interruptFrame.stackSegment = GDT_USER_DATA | 3;
    thread->interruptFrame.flags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    return thread;
}

void thread_free(Thread* thread)
{
    if (atomic_fetch_sub(&thread->process->threadCount, 1) <= 1)
    {
        file_table_cleanup(&thread->process->fileTable);
        address_space_free(thread->process->addressSpace);
        kfree(thread->process);
    }

    kfree(thread);
}