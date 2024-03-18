#include "process.h"

#include <stdatomic.h>
#include <sys/status.h>

#include "heap/heap.h"
#include "lock/lock.h"
#include "vmm/vmm.h"
#include "pmm/pmm.h"
#include "gdt/gdt.h"
#include "debug/debug.h"

static _Atomic uint64_t newPid = 0;

Process* process_new(void)
{
    Process* process = kmalloc(sizeof(Process));
    process->id = atomic_fetch_add(&newPid, 1);
    process->addressSpace = address_space_new();
    process->killed = 0;
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
    thread->kernelStackBottom = kmalloc(PAGE_SIZE);
    thread->kernelStackTop = (void*)((uint64_t)thread->kernelStackBottom + PAGE_SIZE);
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->interruptFrame = interrupt_frame_new(entry, (void*)(VMM_LOWER_HALF_MAX));
    thread->status = STATUS_SUCCESS;
    thread->state = THREAD_STATE_ACTIVE;
    thread->priority = priority;
    thread->boost = 0;

    return thread;
}

void thread_free(Thread* thread)
{
    if (atomic_fetch_sub(&thread->process->threadCount, 1) <= 1)
    {
        address_space_free(thread->process->addressSpace);
        kfree(thread->process);
    }

    interrupt_frame_free(thread->interruptFrame);
    kfree(thread->kernelStackBottom);
    kfree(thread);
}