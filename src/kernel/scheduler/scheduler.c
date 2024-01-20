#include "scheduler.h"

#include "tty/tty.h"
#include "heap/heap.h"
#include "debug/debug.h"
#include "string/string.h"
#include "io/io.h"
#include "idt/idt.h"
#include "time/time.h"
#include "gdt/gdt.h"
#include "apic/apic.h"

Scheduler* schedulers[SMP_MAX_CPU_AMOUNT];

Scheduler* least_loaded_scheduler()
{
    uint64_t shortestLength = -1;
    Scheduler* leastLoadedScheduler = 0;
    for (int i = 0; i < SMP_MAX_CPU_AMOUNT; i++)
    {
        if (schedulers[i] != 0)
        {    
            uint64_t length = queue_length(schedulers[i]->readyQueue) + queue_length(schedulers[i]->expressQueue);
            if (schedulers[i]->runningTask != 0)
            {
                length += 1;
            }

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

            schedulers[i]->cpu = smp_cpu(i);

            schedulers[i]->expressQueue = queue_new();
            schedulers[i]->readyQueue = queue_new();
            schedulers[i]->blockedTasks = vector_new(sizeof(BlockedTask));

            schedulers[i]->runningTask = 0;
            schedulers[i]->lock = spin_lock_new();

            schedulers[i]->nextPreemption = 0;
        }
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_push(Process* process, InterruptFrame* interruptFrame)
{
    Task* newTask = kmalloc(sizeof(Task));
    newTask->process = process;
    newTask->interruptFrame = interruptFrame;
    newTask->state = TASK_STATE_EXPRESS;

    scheduler_acquire_all();

    Scheduler* scheduler = least_loaded_scheduler();
    queue_push(scheduler->expressQueue, newTask);

    scheduler_release_all();
}

void scheduler_acquire_all()
{
    for (int i = 0; i < SMP_MAX_CPU_AMOUNT; i++)
    {
        if (schedulers[i] != 0)
        {    
            spin_lock_acquire(&schedulers[i]->lock);
        }
    }
}

void scheduler_release_all()
{
    for (int i = 0; i < SMP_MAX_CPU_AMOUNT; i++)
    {
        if (schedulers[i] != 0)
        {    
            spin_lock_release(&schedulers[i]->lock);
        }
    }
}

Scheduler* scheduler_get_local()
{
    return schedulers[smp_current_cpu()->id];
}

void local_scheduler_tick(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_get_local();

    //TODO: Implement priority system and priority preemption

    if (scheduler->nextPreemption < time_nanoseconds())
    {
        local_scheduler_schedule(interruptFrame);
    }
}

void local_scheduler_schedule(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_get_local();

    Task* newTask = 0;
    if (queue_length(scheduler->expressQueue) != 0)
    {
        newTask = queue_pop(scheduler->expressQueue);
    }
    else if (queue_length(scheduler->readyQueue) != 0)
    {
        newTask = queue_pop(scheduler->readyQueue);
    }

    if (newTask != 0)
    {
        if (scheduler->runningTask != 0)
        {
            Task* oldTask = scheduler->runningTask;
            interrupt_frame_copy(oldTask->interruptFrame, interruptFrame);

            oldTask->state = TASK_STATE_READY;
            queue_push(scheduler->readyQueue, oldTask);                
        }

        newTask->state = TASK_STATE_RUNNING;
        scheduler->runningTask = newTask;

        interrupt_frame_copy(interruptFrame, newTask->interruptFrame);

        scheduler->nextPreemption = time_nanoseconds() + NANOSECONDS_PER_SECOND / 2;
    }
    else if (scheduler->runningTask == 0)
    {
        scheduler->runningTask = 0;
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->cr3 = (uint64_t)kernelPageDirectory;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->stackPointer = tss_get(smp_current_cpu()->id)->rsp0;

        scheduler->nextPreemption = time_nanoseconds() + NANOSECONDS_PER_MILLISECOND;
    }
    else
    {
        // Keep running same task
        scheduler->nextPreemption = time_nanoseconds() + NANOSECONDS_PER_SECOND / 2;
    }
}

void local_scheduler_block(InterruptFrame* interruptFrame, Blocker blocker)
{

}

void local_scheduler_exit()
{
    Scheduler* scheduler = scheduler_get_local();

    process_free(scheduler->runningTask->process);
    interrupt_frame_free(scheduler->runningTask->interruptFrame);
    kfree(scheduler->runningTask);

    scheduler->runningTask = 0;
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