#include "scheduler.h"

#include "heap/heap.h"
#include "tty/tty.h"
#include "gdt/gdt.h"
#include "debug/debug.h"
#include "worker_pool/worker_pool.h"

Scheduler* scheduler_new()
{
    Scheduler* scheduler = kmalloc(sizeof(Scheduler));

    for (uint64_t priority = TASK_PRIORITY_MIN; priority <= TASK_PRIORITY_MAX; priority++)
    {
        scheduler->queues[priority] = queue_new();
    }
    scheduler->runningTask = 0;

    scheduler->blockedTasks = vector_new(sizeof(BlockedTask));

    scheduler->nextPreemption = 0;
    scheduler->lock = lock_new();

    return scheduler;
}

void scheduler_acquire(Scheduler* scheduler)
{
    lock_acquire(&scheduler->lock);
}

void scheduler_release(Scheduler* scheduler)
{
    lock_release(&scheduler->lock);
}

void scheduler_push(Scheduler* scheduler, Task* task)
{
    if (task->priority < TASK_PRIORITY_MAX)
    {
        queue_push(scheduler->queues[task->priority + 1], task);
    }
    else
    {
        queue_push(scheduler->queues[task->priority], task);
    }
}

void scheduler_exit(Scheduler* scheduler)
{
    task_free(scheduler->runningTask);
    scheduler->runningTask = 0;
}

void scheduler_schedule(Scheduler* scheduler, InterruptFrame* interruptFrame)
{        
    Task* newTask = 0;
    for (int64_t i = TASK_PRIORITY_MAX; i >= 0; i--) 
    {
        if (queue_length(scheduler->queues[i]) != 0)
        {
            newTask = queue_pop(scheduler->queues[i]);
            break;
        }
    }        

    if (newTask != 0)
    {
        if (scheduler->runningTask != 0)
        {
            Task* oldTask = scheduler->runningTask;
            interrupt_frame_copy(oldTask->interruptFrame, interruptFrame);

            oldTask->state = TASK_STATE_READY;
            queue_push(scheduler->queues[oldTask->priority], oldTask);                
        }

        newTask->state = TASK_STATE_RUNNING;
        scheduler->runningTask = newTask;

        interrupt_frame_copy(interruptFrame, newTask->interruptFrame);

        scheduler->nextPreemption = time_nanoseconds() + SCHEDULER_TIME_SLICE;
    }
    else if (scheduler->runningTask == 0)
    {
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->cr3 = (uint64_t)kernelPageDirectory;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->stackPointer = worker_self()->tss->rsp0;
    }
}

void scheduler_block(Scheduler* scheduler, InterruptFrame* interruptFrame, Blocker blocker)
{
    BlockedTask newBlockedTask = 
    {
        .blocker = blocker,
        .task = scheduler->runningTask
    };

    interrupt_frame_copy(scheduler->runningTask->interruptFrame, interruptFrame);
    scheduler->runningTask = 0;

    uint8_t taskInserted = 0;
    for (uint64_t i = 0; i < vector_length(scheduler->blockedTasks); i++)
    {
        BlockedTask* blockedTask = vector_get(scheduler->blockedTasks, i);
        if (blockedTask->blocker.timeout > newBlockedTask.blocker.timeout)
        {
            vector_insert(scheduler->blockedTasks, i, &newBlockedTask);
            taskInserted = 1;
            break;
        }
    }

    if (!taskInserted)
    {
        vector_push(scheduler->blockedTasks, &newBlockedTask);
    }
}

void scheduler_unblock(Scheduler* scheduler)
{
    if (vector_length(scheduler->blockedTasks) != 0)
    {
        BlockedTask* blockedTask = vector_get(scheduler->blockedTasks, 0);

        if (blockedTask->blocker.timeout <= time_nanoseconds())
        {
            scheduler_push(scheduler, blockedTask->task);
            vector_erase(scheduler->blockedTasks, 0);
        }
    }
}

uint8_t scheduler_wants_to_schedule(Scheduler* scheduler)
{
    //If time slice is over and task is available
    if (scheduler->nextPreemption < time_nanoseconds())
    {
        for (int64_t i = TASK_PRIORITY_MAX; i >= 0; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                return 1;
            }
        }
    }
    
    //If higher priority task is available
    if (scheduler->runningTask != 0)
    {
        uint8_t runningPriority = scheduler->runningTask->priority;
        for (int64_t i = TASK_PRIORITY_MAX; i > runningPriority; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                return 1;
            }
        }
    }

    return 0;
}