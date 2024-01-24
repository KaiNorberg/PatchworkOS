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

Scheduler* schedulers;

Scheduler* least_loaded_scheduler()
{
    uint64_t shortestLength = -1;
    Scheduler* leastLoadedScheduler = 0;
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        uint64_t length = (schedulers[i].runningTask != 0);
        for (int64_t priority = TASK_PRIORITY_MAX; priority >= TASK_PRIORITY_MIN; priority--) 
        {
            length += queue_length(schedulers[i].queues[priority]);
        }

        if (shortestLength > length)
        {
            shortestLength = length;
            leastLoadedScheduler = &schedulers[i];
        }
    }

    return leastLoadedScheduler;
}

void scheduler_init()
{
    tty_start_message("Scheduler initializing");

    schedulers = kmalloc(sizeof(Scheduler) * smp_cpu_amount());

    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        schedulers[i].cpu = smp_cpu(i);

        for (uint64_t priority = TASK_PRIORITY_MIN; priority <= TASK_PRIORITY_MAX; priority++)
        {
            schedulers[i].queues[priority] = queue_new();
        }
        schedulers[i].runningTask = 0;

        schedulers[i].blockedTasks = vector_new(sizeof(BlockedTask));

        schedulers[i].nextPreemption = 0;
        schedulers[i].lock = spin_lock_new();
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_acquire_all()
{
    for (int i = 0; i < smp_cpu_amount(); i++)
    {  
        spin_lock_acquire(&schedulers[i].lock);
    }
}

void scheduler_release_all()
{
    for (int i = 0; i < smp_cpu_amount(); i++)
    {
        spin_lock_release(&schedulers[i].lock);
    }
}

void scheduler_push(Task* task)
{
    scheduler_acquire_all();

    Scheduler* scheduler = least_loaded_scheduler();

    if (task->priority < TASK_PRIORITY_MAX)
    {
        queue_push(scheduler->queues[task->priority + 1], task);
    }
    else
    {
        queue_push(scheduler->queues[task->priority], task);
    }

    scheduler_release_all();
}

Scheduler* scheduler_get_local()
{
    return &schedulers[smp_current_cpu()->id];
}

Scheduler* scheduler_get(uint8_t cpuId)
{
    return &schedulers[cpuId];
}

void local_scheduler_push(Task* task)
{
    Scheduler* scheduler = scheduler_get_local();

    if (task->priority < TASK_PRIORITY_MAX)
    {
        queue_push(scheduler->queues[task->priority + 1], task);
    }
    else
    {
        queue_push(scheduler->queues[task->priority], task);
    }
}

void local_scheduler_tick(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_get_local();

    if (vector_length(scheduler->blockedTasks) != 0)
    {
        BlockedTask* blockedTask = vector_get(scheduler->blockedTasks, 0);

        if (blockedTask->blocker.timeout <= time_nanoseconds())
        {
            local_scheduler_push(blockedTask->task);
            vector_erase(scheduler->blockedTasks, 0);
        }
    }

    if (scheduler->nextPreemption < time_nanoseconds())
    {
        local_scheduler_schedule(interruptFrame);
    }
    else if (scheduler->runningTask != 0)
    {
        uint8_t runningPriority = scheduler->runningTask->priority;
        for (int64_t i = TASK_PRIORITY_MAX; i > runningPriority; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                local_scheduler_schedule(interruptFrame);
                break;
            }
        }
    }
}

void local_scheduler_schedule(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_get_local();
        
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
        interruptFrame->stackPointer = tss_get(smp_current_cpu()->id)->rsp0;

        scheduler->nextPreemption = 0;
    }
    else
    {
        //Keep running the same task, change to 0?
        scheduler->nextPreemption = time_nanoseconds() + SCHEDULER_TIME_SLICE;
    }
}

void local_scheduler_block(InterruptFrame* interruptFrame, Blocker blocker)
{    
    Scheduler* scheduler = scheduler_get_local();

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

void local_scheduler_exit()
{
    Scheduler* scheduler = scheduler_get_local();

    task_free(scheduler->runningTask);
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

uint64_t local_scheduler_task_amount()
{
    Scheduler* scheduler = scheduler_get_local();

    uint64_t amount = 0;
    for (uint64_t i = TASK_PRIORITY_MIN; i <= TASK_PRIORITY_MAX; i++)
    {
        amount += queue_length(scheduler->queues[i]);
    }
    if (scheduler->runningTask != 0)
    {
        amount += 1;
    }
    
    return amount;
}

Task* local_scheduler_running_task()
{
    return scheduler_get_local()->runningTask;
}