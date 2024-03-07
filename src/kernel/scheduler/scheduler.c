#include "scheduler.h"

#include "vmm/vmm.h"
#include "smp/smp.h"
#include "heap/heap.h"
#include "gdt/gdt.h"
#include "time/time.h"
#include "debug/debug.h"
#include "interrupts/interrupts.h"
#include "irq/irq.h"

#include "program_loader/program_loader.h"

#include <libc/string.h>

static atomic_size_t tid = 0;

static Scheduler** schedulers;

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

static uint8_t scheduler_wants_to_schedule(Scheduler* scheduler)
{   
    if (interrupt_depth() != 0)
    {        
        //Cant schedule
        return 0;
    }
    else if (scheduler->runningThread == 0)
    {
        for (int64_t i = THREAD_PRIORITY_MIN; i <= THREAD_PRIORITY_MAX; i++) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                return 1;
            }
        }
    }
    else if (scheduler->runningThread->timeEnd < time_nanoseconds())
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i >= 0; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                return 1;
            }
        }
    }
    else
    {
        uint8_t runningPriority = scheduler->runningThread->priority;
        for (int64_t i = THREAD_PRIORITY_MAX; i > runningPriority; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                return 1;
            }
        }
    }

    return 0;
}

void scheduler_init()
{
    irq_install_handler(scheduler_irq_handler, IRQ_TIMER);

    schedulers = kmalloc(sizeof(Scheduler*) * smp_cpu_amount());
    memset(schedulers, 0, sizeof(Scheduler*) * smp_cpu_amount());

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

Thread* scheduler_thread()
{
    Thread* thread = schedulers[smp_self()->id]->runningThread;
    smp_put();

    return thread;
}

Process* scheduler_process()
{
    Process* process = schedulers[smp_self()->id]->runningThread->common->process;
    smp_put();

    return process;
}

void scheduler_invoke()
{
    Ipi ipi = 
    {
        .type = IPI_TYPE_SCHEDULE
    };
    smp_send_ipi_to_self(ipi);
}

void scheduler_exit(Status status)
{
    Scheduler* scheduler = schedulers[smp_self()->id];
    lock_acquire(&scheduler->lock);

    Thread* thread = scheduler_thread();

    lock_acquire(&thread->common->lock);
    thread->common->threadCount--;
    if (thread->common->threadCount == 0)
    {
        process_free(thread->common->process);
        kfree(thread->common);
    }
    lock_release(&thread->common->lock);

    interrupt_frame_free(thread->interruptFrame);
    kfree(thread);
    scheduler->runningThread = 0;

    lock_release(&scheduler->lock);
    smp_put();

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
    thread->common = kmalloc(sizeof(ThreadCommon));
    thread->common->process = process;
    thread->common->threadCount = 1;
    thread->common->lock = lock_new();
    thread->id = atomic_fetch_add_explicit(&tid, 1, memory_order_seq_cst);
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->kernelStackBottom = vmm_allocate(1);
    thread->kernelStackTop = (void*)((uint64_t)thread->kernelStackBottom + 0xFFF);
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

    uint64_t bestLength = -1;
    Scheduler* bestScheduler = 0;
    for (uint8_t i = 0; i < smp_cpu_amount(); i++)
    {   
        Cpu const* cpu = smp_cpu(i);

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
    Cpu* self = smp_self();
    Scheduler* scheduler = schedulers[self->id];

    lock_acquire(&scheduler->lock);

    if (!scheduler_wants_to_schedule(scheduler))
    {    
        lock_release(&scheduler->lock);
        interrupts_enable();
        return;
    }

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
        thread->timeStart = time_nanoseconds();
        thread->timeEnd = thread->timeStart + SCHEDULER_TIME_SLICE;
        scheduler->runningThread = thread;

        interrupt_frame_copy(interruptFrame, thread->interruptFrame);

        PAGE_DIRECTORY_LOAD(thread->common->process->pageDirectory);
        self->tss->rsp0 = (uint64_t)thread->kernelStackTop;
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
    lock_release(&scheduler->lock);

    smp_put();
}

//Temporary
uint64_t scheduler_local_thread_amount()
{
    Cpu const* self = smp_self();
    Scheduler* scheduler = schedulers[self->id];
    
    lock_acquire(&scheduler->lock);
    uint64_t length = (scheduler->runningThread != 0);
    for (int64_t priority = THREAD_PRIORITY_MAX; priority >= THREAD_PRIORITY_MIN; priority--) 
    {
        length += queue_length(scheduler->queues[priority]);
    }
    lock_release(&scheduler->lock);

    smp_put();

    return length;
}