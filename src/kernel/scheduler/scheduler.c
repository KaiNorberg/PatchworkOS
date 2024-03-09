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

#include "program_loader/program_loader.h"

#include <libc/string.h>

static _Atomic uint64_t newTid = 0;

static Scheduler** schedulers;

//Must have a corresponding call to scheduler_release()
static inline Scheduler* scheduler_acquire(uint64_t id)
{
    Scheduler* scheduler = schedulers[id];
    lock_acquire(&scheduler->lock);
    return scheduler;
}

static inline void scheduler_release(Scheduler* scheduler)
{
    lock_release(&scheduler->lock);
}

//Must have a corresponding call to scheduler_put()
static inline Scheduler* scheduler_self()
{
    Scheduler* scheduler = schedulers[smp_self()->id];
    lock_acquire(&scheduler->lock);
    return scheduler;
}

static inline void scheduler_put()
{
    lock_release(&schedulers[smp_self_unsafe()->id]->lock);
    smp_put();
}

static void scheduler_enqueue(Thread* thread)
{
    uint64_t bestLength = UINT64_MAX;
    uint64_t best = 0;
    //for (int8_t i =  smp_cpu_amount() - 1; i >= 0; i--)
    for (uint8_t i = 0; i < smp_cpu_amount(); i++)
    {   
        Scheduler* scheduler = scheduler_acquire(i);

        uint64_t length = (scheduler->runningThread != 0);
        for (int64_t priority = THREAD_PRIORITY_MAX; priority >= THREAD_PRIORITY_MIN; priority--) 
        {
            length += queue_length(scheduler->queues[priority]);
        }

        if (bestLength > length)
        {
            bestLength = length;
            best = i;
        }    

        scheduler_release(scheduler);
    }

    Scheduler* scheduler = scheduler_acquire(best);
    if (thread->priority < THREAD_PRIORITY_MAX)
    {
        queue_push(scheduler->queues[thread->priority + 1], thread);
    }
    else
    {
        queue_push(scheduler->queues[thread->priority], thread);
    }
    scheduler_release(scheduler);
}

static void scheduler_invoke()
{
    Scheduler* scheduler = scheduler_self();

    Ipi ipi = 
    {
        .type = IPI_TYPE_SCHEDULE
    };
    smp_send_ipi_to_self(ipi);    
    scheduler_put();
}

static void scheduler_irq_handler(uint8_t irq)
{
    switch (irq)
    {
    case IRQ_TIMER:
    {
        Ipi ipi = 
        {
            .type = IPI_TYPE_SCHEDULE
        };
        smp_send_ipi_to_others(ipi);
    }
    break;
    default:
    {
        debug_panic("Scheduler invalid IRQ");
    }
    break;
    }
}

void scheduler_init()
{
    irq_install_handler(scheduler_irq_handler, IRQ_TIMER);

    schedulers = kmalloc(sizeof(Scheduler*) * smp_cpu_amount());
    memset(schedulers, 0, sizeof(Scheduler*) * smp_cpu_amount());

    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        Scheduler* scheduler = kmalloc(sizeof(Scheduler));
        for (uint64_t priority = THREAD_PRIORITY_MIN; priority <= THREAD_PRIORITY_MAX; priority++)
        {
            scheduler->queues[priority] = queue_new();
        }
        scheduler->runningThread = 0;
        scheduler->lock = lock_new();

        schedulers[i] = scheduler;
    }
}

Thread* scheduler_thread()
{
    Thread* thread = schedulers[smp_self()->id]->runningThread;
    smp_put();

    return thread;
}

Process* scheduler_process()
{
    Process* process = schedulers[smp_self()->id]->runningThread->process;
    smp_put();

    return process;
}

void scheduler_exit(Status status)
{
    Scheduler* scheduler = scheduler_self();

    Thread* thread = scheduler_thread();
    
    process_unref(thread->process);
    interrupt_frame_free(thread->interruptFrame);
    kfree(thread);

    scheduler->runningThread = 0;

    scheduler_put();
    scheduler_invoke();
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
    thread->interruptFrame = interrupt_frame_new(program_loader_entry, (void*)(VMM_LOWER_HALF_MAX - 1));
    thread->status = STATUS_SUCCESS;
    thread->state = THREAD_STATE_READY;
    thread->priority = THREAD_PRIORITY_MIN;

    //Temporary: For now the executable is passed via the user stack to the program loader.
    //Eventually it will be passed via a system similar to "/proc/self/exec".
    void* stackBottom = process_allocate_pages(process, (void*)(VMM_LOWER_HALF_MAX - PAGE_SIZE), 1);
    void* stackTop = (void*)((uint64_t)stackBottom + 0xFFF);
    uint64_t pathLength = strlen(path);
    void* dest = (void*)((uint64_t)stackTop - pathLength - 1);
    memcpy(dest, path, pathLength + 1);
    thread->interruptFrame->stackPointer -= pathLength + 1;
    thread->interruptFrame->rdi = VMM_LOWER_HALF_MAX - 1 - pathLength - 1;

    scheduler_enqueue(thread);

    return process->id;
}

void scheduler_schedule(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_self();

    if (interrupt_depth() != 0)
    {    
        scheduler_put();
        return;
    }

    Cpu* self = smp_self_unsafe();

    Thread* thread = 0;  
    if (scheduler->runningThread != 0 && scheduler->runningThread->timeEnd > time_nanoseconds())
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i > scheduler->runningThread->priority; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                thread = queue_pop(scheduler->queues[i]);
                break;
            }
        }
    }
    else
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i >= THREAD_PRIORITY_MIN; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                thread = queue_pop(scheduler->queues[i]);
                break;
            }
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
        thread->timeStart = time_nanoseconds();
        thread->timeEnd = thread->timeStart + SCHEDULER_TIME_SLICE;
        interrupt_frame_copy(interruptFrame, thread->interruptFrame);

        PAGE_DIRECTORY_LOAD(thread->process->pageDirectory);
        self->tss->rsp0 = (uint64_t)thread->kernelStackTop;

        scheduler->runningThread = thread;
    }
    else if (scheduler->runningThread == 0)
    {
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->stackPointer = (uint64_t)self->idleStackTop;

        PAGE_DIRECTORY_LOAD(vmm_kernel_directory());
        self->tss->rsp0 = (uint64_t)self->idleStackTop;
    }
    else
    {
        //Keep running same thread
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