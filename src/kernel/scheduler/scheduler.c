#include "scheduler.h"

#include "vmm/vmm.h"
#include "smp/smp.h"
#include "heap/heap.h"
#include "gdt/gdt.h"
#include "time/time.h"
#include "debug/debug.h"
#include "registers/registers.h"
#include "interrupts/interrupts.h"
#include "irq/irq.h"
#include "apic/apic.h"

#include "program_loader/program_loader.h"

#include <libc/string.h>

static _Atomic uint64_t newTid = 0;

static Scheduler** schedulers;

//Must have a corresponding call to scheduler_put()
static inline Scheduler* scheduler_self()
{
    return schedulers[smp_self()->id];
}

static inline void scheduler_put()
{
    smp_put();
}

static void scheduler_enqueue(Thread* thread, uint8_t boost, uint16_t preferred)
{
    int64_t bestLength = INT64_MAX;
    uint64_t best = 0;
    for (int8_t i = smp_cpu_amount() - 1; i >= 0; i--)
    {   
        int64_t length = (int64_t)(schedulers[i]->runningThread != 0);
        for (int64_t priority = THREAD_PRIORITY_MAX; priority >= THREAD_PRIORITY_MIN; priority--) 
        {
            length += queue_length(schedulers[i]->queues[priority]);
        }
        
        if (i == preferred)
        {
            length--;
        }

        if (bestLength > length)
        {
            bestLength = length;
            best = i;
        }    
    }
    
    thread->boost = thread->priority + boost <= THREAD_PRIORITY_MAX ? boost : 0;
    queue_push(schedulers[best]->queues[thread->priority + thread->boost], thread);
}

void scheduler_init()
{
    schedulers = kmalloc(sizeof(Scheduler*) * smp_cpu_amount());
    memset(schedulers, 0, sizeof(Scheduler*) * smp_cpu_amount());

    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        Scheduler* scheduler = kmalloc(sizeof(Scheduler));
        scheduler->id = i;
        for (uint64_t priority = THREAD_PRIORITY_MIN; priority <= THREAD_PRIORITY_MAX; priority++)
        {
            scheduler->queues[priority] = queue_new();
        }
        scheduler->runningThread = 0;

        schedulers[i] = scheduler;
    }
}

void scheduler_cpu_start()
{
    apic_timer_init(IPI_BASE + IPI_SCHEDULE, SCHEDULER_TIMER_HZ);
}

Thread* scheduler_thread()
{
    Thread* thread = scheduler_self()->runningThread;
    scheduler_put();

    return thread;
}

Process* scheduler_process()
{
    Process* process = scheduler_self()->runningThread->process;
    scheduler_put();

    return process;
}

void scheduler_exit(Status status)
{
    Scheduler* scheduler = scheduler_self();
    scheduler->runningThread->state = THREAD_STATE_KILLED;

    //TODO: Deallocate thread, stack?
    /*process_unref(thread->process);
    interrupt_frame_free(thread->interruptFrame);
    //vmm_free(thread->kernelStackBottom, 1);
    kfree(thread);*/

    scheduler_put();
    scheduler_yield();
}

int64_t scheduler_spawn(const char* path)
{    
    //This sure is messy...
    Process* process = process_new();
    if (process == 0)
    {
        return -1;
    }

    Thread* thread = kmalloc(sizeof(Thread));
    thread->process = process;
    thread->id = atomic_fetch_add(&newTid, 1);
    thread->kernelStackBottom = vmm_allocate(1);
    thread->kernelStackTop = (void*)((uint64_t)thread->kernelStackBottom + PAGE_SIZE);
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->interruptFrame = interrupt_frame_new(program_loader_entry, (void*)(VMM_LOWER_HALF_MAX));
    thread->status = STATUS_SUCCESS;
    thread->state = THREAD_STATE_ACTIVE;
    thread->priority = THREAD_PRIORITY_MIN;

    //Temporary: For now the executable is passed via the user stack to the program loader.
    //Eventually it will be passed via a system similar to "/proc/self/exec".
    void* stackBottom = process_allocate_pages(process, (void*)(VMM_LOWER_HALF_MAX - PAGE_SIZE), 1);
    void* stackTop = (void*)((uint64_t)stackBottom + PAGE_SIZE);
    uint64_t pathLength = strlen(path);
    void* dest = (void*)((uint64_t)stackTop - pathLength - 1);
    memcpy(dest, path, pathLength + 1);
    thread->interruptFrame->stackPointer -= pathLength + 1;
    thread->interruptFrame->rdi = VMM_LOWER_HALF_MAX - pathLength - 1;

    scheduler_enqueue(thread, 5, -1);

    return process->id;
}

void scheduler_schedule(InterruptFrame* interruptFrame)
{
    //This sure is messy...

    Scheduler* scheduler = scheduler_self();

    if (interrupt_depth() != 0)
    {    
        scheduler_put();
        return; 
    }

    if (scheduler->runningThread != 0)
    {
        switch (scheduler->runningThread->state)
        {
        case THREAD_STATE_ACTIVE:
        {

        }
        break;
        case THREAD_STATE_KILLED:
        {
            scheduler->runningThread = 0;
        }
        break;
        }
    }

    Thread* thread = 0;  
    if (scheduler->runningThread != 0 && scheduler->runningThread->timeEnd > time_nanoseconds())
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i > scheduler->runningThread->priority + scheduler->runningThread->boost; i--) 
        {
            thread = queue_pop(scheduler->queues[i]);
            if (thread != 0)
            {
                break;
            }
        }
    }
    else
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i >= THREAD_PRIORITY_MIN; i--) 
        {
            thread = queue_pop(scheduler->queues[i]);
            if (thread != 0)
            {
                break;
            }
        }
    }

    Cpu* self = smp_self_unsafe();
    if (thread != 0)
    {
        if (scheduler->runningThread != 0)
        {
            interrupt_frame_copy(scheduler->runningThread->interruptFrame, interruptFrame);

            scheduler_enqueue(scheduler->runningThread, 0, scheduler->id);
            scheduler->runningThread = 0;
        }

        thread->timeStart = time_nanoseconds();
        thread->timeEnd = thread->timeStart + SCHEDULER_TIME_SLICE;
        interrupt_frame_copy(interruptFrame, thread->interruptFrame);
        
        page_directory_load(thread->process->pageDirectory);
        self->tss->rsp0 = (uint64_t)thread->kernelStackTop;

        scheduler->runningThread = thread;
    }
    else if (scheduler->runningThread == 0) //Idle
    {
        memset(interruptFrame, 0, sizeof(InterruptFrame));
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->flags = 0x202;
        interruptFrame->stackPointer = (uint64_t)self->idleStackTop;

        page_directory_load(vmm_kernel_directory());
        self->tss->rsp0 = (uint64_t)self->idleStackTop;
    }
    else
    {
        //Keep running the same thread
    }

    scheduler_put();
}

//Temporary
uint64_t scheduler_local_thread_amount()
{
    Scheduler const* scheduler = scheduler_self();
    
    uint64_t length = (scheduler->runningThread != 0);
    for (int64_t priority = THREAD_PRIORITY_MAX; priority >= THREAD_PRIORITY_MIN; priority--) 
    {
        length += queue_length(scheduler->queues[priority]);
    }

    scheduler_put();

    return length;
}