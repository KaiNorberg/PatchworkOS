#include "kernel_process.h"

#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "gdt/gdt.h"
#include "global_heap/global_heap.h"
#include "scheduler/scheduler.h"
#include "debug/debug.h"
#include "time/time.h"
#include "syscall/syscall.h"
#include "tty/tty.h"

Process* kernelProcess;

void kernel_process_init()
{
    kernelProcess = kmalloc(sizeof(Process));

    kernelProcess->pageDirectory = kernelPageDirectory;
    kernelProcess->firstMemoryBlock = 0;
    kernelProcess->lastMemoryBlock = 0;
    kernelProcess->id = pid_new();
}

Task* kernel_task_new(void* entry)
{
    void* stackPointer = page_allocator_request();

    InterruptFrame* interruptFrame = interrupt_frame_new(kernel_task_entry, 
        stackPointer, GDT_KERNEL_CODE, GDT_KERNEL_DATA, 0x202, kernelProcess->pageDirectory);
    interruptFrame->rdi = (uint64_t)entry;

    Task* kernelTask = kmalloc(sizeof(Task));
    kernelTask->process = kernelProcess;
    kernelTask->interruptFrame = interruptFrame;
    kernelTask->state = TASK_STATE_READY;
    kernelTask->priority = TASK_PRIORITY_MAX;

    return kernelTask;
}

void kernel_task_block_handler(InterruptFrame* interruptFrame)
{
    local_scheduler_acquire();

    Blocker blocker =
    {
        .timeout = interruptFrame->rdi
    };

    local_scheduler_block(interruptFrame, blocker);
    local_scheduler_schedule(interruptFrame);
    local_scheduler_release();
}