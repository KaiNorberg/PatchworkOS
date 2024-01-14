#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"
#include "idt/idt.h"
#include "smp/smp.h"
#include "time/time.h"
#include "gdt/gdt.h"

Scheduler* schedulers[SMP_MAX_CPU_AMOUNT];

Scheduler* least_loaded_scheduler()
{
    uint64_t shortestLength = -1;
    Scheduler* leastLoadedScheduler = 0;
    for (int i = 0; i < SMP_MAX_CPU_AMOUNT; i++)
    {
        if (schedulers[i] != 0)
        {    
            spin_lock_acquire(&schedulers[i]->lock);
            uint64_t length = queue_length(schedulers[i]->readyQueue);
            spin_lock_release(&schedulers[i]->lock);

            if (shortestLength > length)
            {
                shortestLength = length;
                leastLoadedScheduler = schedulers[i];
            }
        }
    }

    return leastLoadedScheduler;
}

void scheduler_init()
{
    tty_start_message("Scheduler initializing");
    memclr(schedulers, sizeof(Scheduler*) * SMP_MAX_CPU_AMOUNT);

    for (int i = 0; i < SMP_MAX_CPU_AMOUNT; i++)
    {
        if (smp_cpu(i)->present)
        {
            schedulers[i] = kmalloc(sizeof(Scheduler));

            schedulers[i]->readyQueue = queue_new();
            schedulers[i]->runningTask = 0;
            schedulers[i]->lock = spin_lock_new();
        }
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_push(Process* process, InterruptFrame* interruptFrame)
{
    Task* newTask = kmalloc(sizeof(Task));
    newTask->process = process;
    newTask->interruptFrame = interruptFrame;
    newTask->state = TASK_STATE_READY;

    Scheduler* scheduler = least_loaded_scheduler();

    spin_lock_acquire(&scheduler->lock);
    queue_push(scheduler->readyQueue, newTask); 
    spin_lock_release(&scheduler->lock);   
}

Scheduler* scheduler_get_local()
{
    return schedulers[smp_current_cpu()->id];
}

void local_scheduler_schedule(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_get_local();

    if (scheduler->runningTask != 0)
    {
        Task* oldTask = scheduler->runningTask;
        oldTask->state = TASK_STATE_READY;
        interrupt_frame_copy(oldTask->interruptFrame, interruptFrame);

        queue_push(scheduler->readyQueue, oldTask);
    }

    if (queue_length(scheduler->readyQueue) != 0)
    {
        Task* runningTask = queue_pop(scheduler->readyQueue);
        runningTask->state = TASK_STATE_RUNNING;
        scheduler->runningTask = runningTask;

        interrupt_frame_copy(interruptFrame, runningTask->interruptFrame);

        scheduler->nextPreemption = time_nanoseconds() + NANOSECONDS_PER_SECOND / 2;
    }
    else
    {
        scheduler->runningTask = 0;
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->cr3 = (uint64_t)kernelPageDirectory;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->stackPointer = tss_get(smp_current_cpu()->id)->rsp0;

        scheduler->nextPreemption = time_nanoseconds() + NANOSECONDS_PER_MILLISECOND;
    }
}

void local_scheduler_exit()
{
    Scheduler* scheduler = scheduler_get_local();

    spin_lock_acquire(&scheduler->lock);

    process_free(scheduler->runningTask->process);
    interrupt_frame_free(scheduler->runningTask->interruptFrame);
    kfree(scheduler->runningTask);

    scheduler->runningTask = 0;

    spin_lock_release(&scheduler->lock);
}

void local_scheduler_acquire()
{
    spin_lock_acquire(&scheduler_get_local()->lock);
}

void local_scheduler_release()
{
    spin_lock_release(&scheduler_get_local()->lock);
}

uint64_t local_scheduler_deadline()
{
    return scheduler_get_local()->nextPreemption;
}

Task* local_scheduler_running_task()
{
    return scheduler_get_local()->runningTask;
}