#include "scheduler.h"

#include "vmm/vmm.h"
#include "smp/smp.h"
#include "heap/heap.h"
#include "gdt/gdt.h"

#include "program_loader/program_loader.h"

#include <libc/string.h>

static Scheduler* schedulers[MAX_CPU_AMOUNT];

void scheduler_init()
{
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        Cpu const* cpu = smp_cpu(i);

        Scheduler* scheduler = kmalloc(sizeof(Scheduler));
        for (uint64_t priority = THREAD_PRIORITY_MIN; priority <= THREAD_PRIORITY_MAX; priority++)
        {
            scheduler->queues[priority] = queue_new();
        }
        scheduler->runningThread = 0;
        scheduler->lock = lock_new();

        schedulers[cpu->id] = scheduler;
    }
}

Thread* scheduler_self()
{
    Cpu* self = smp_acquire();
    Thread* thread = schedulers[self->id]->runningThread;
    smp_release();

    return thread;
}

int64_t scheduler_spawn(const char* path)
{
    Process* process = process_new(path);
    if (process == 0)
    {
        return -1;
    }

    Thread* thread = kmalloc(sizeof(Thread));
    thread->process = process;
    thread->data = kmalloc(sizeof(ThreadData));
    thread->data->threadCount = 1;
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->kernelStackBottom = vmm_allocate(1);
    thread->kernelStackTop = (void*)((uint64_t)thread->kernelStackBottom + 0xFFF);
    thread->interruptFrame = 
        interrupt_frame_new(program_loader_entry, (void*)(VMM_LOWER_HALF_MAX - 1), process->pageDirectory);
    thread->status = STATUS_SUCCESS;
    thread->state = THREAD_STATE_READY;
    thread->priority = THREAD_PRIORITY_MIN;

    //Temporary: For now the executable is passed via the stack to the program loader.
    //Eventually it will be passed via a system similar to "/proc/self/exec".
    void* stackBottom = process_allocate_pages(process, (void*)(VMM_LOWER_HALF_MAX - PAGE_SIZE), 1);
    void* stackTop = (void*)((uint64_t)stackBottom + 0xFFF);
    uint64_t pathLength = strlen(path);
    void* dest = (void*)((uint64_t)stackTop - pathLength - 1);
    memcpy(dest, path, pathLength + 1);
    thread->interruptFrame->stackPointer -= pathLength + 1;
    thread->interruptFrame->rdi = VMM_LOWER_HALF_MAX - 1 - pathLength - 1;

    uint64_t bestLength = -1;
    Scheduler* bestScheduler = 0;
    for (uint8_t i = 0; i < smp_cpu_amount(); i++)
    {   
        Cpu* cpu = smp_cpu(i);     

        Scheduler* scheduler = schedulers[cpu->id];
        lock_acquire(&scheduler->lock);

        uint64_t length = (scheduler->runningThread != 0);
        for (int64_t priority = THREAD_PRIORITY_MAX; priority >= THREAD_PRIORITY_MIN; priority--) 
        {
            length += queue_length(scheduler->queues[priority]);
        }

        if (bestLength > length)
        {
            bestLength = length;
            bestScheduler = scheduler;
        }    

        lock_release(&scheduler->lock);
    }

    if (bestScheduler == 0)
    {
        return -1;
    }

    lock_acquire(&bestScheduler->lock);
    queue_push(bestScheduler->queues[thread->priority], thread);
    lock_release(&bestScheduler->lock);

    return process->id;
}

void scheduler_schedule(InterruptFrame* interruptFrame)
{
    Cpu* self = smp_acquire();

    Scheduler* scheduler = schedulers[self->id];
    lock_acquire(&scheduler->lock);

    Thread* thread = 0;
    for (int64_t i = THREAD_PRIORITY_MAX; i >= THREAD_PRIORITY_MIN; i--) 
    {
        if (queue_length(scheduler->queues[i]) != 0)
        {
            thread = queue_pop(scheduler->queues[i]);
            break;
        }
    }        

    if (thread != 0)
    {
        if (scheduler->runningThread != 0)
        {
            interrupt_frame_copy(scheduler->runningThread->interruptFrame, interruptFrame);

            scheduler->runningThread->state = THREAD_STATE_READY;
            queue_push(scheduler->queues[scheduler->runningThread->priority], scheduler->runningThread);                
        }

        thread->state = THREAD_STATE_RUNNING;
        scheduler->runningThread = thread;

        interrupt_frame_copy(interruptFrame, thread->interruptFrame);
        self->tss->rsp0 = (uint64_t)thread->kernelStackTop;
        //gdt_load_tss(self->tss);

        //scheduler->nextPreemption = time_nanoseconds() + SCHEDULER_TIME_SLICE;
    }
    else if (scheduler->runningThread == 0)
    {
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->cr3 = (uint64_t)vmm_kernel_directory();
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->stackPointer = (uint64_t)self->idleStackTop;
    }

    lock_release(&scheduler->lock);
    smp_release();
}