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

Vector* blockedTasks;
uint64_t nextBalancing;
SpinLock schedulerLock;

Scheduler* schedulers;

Scheduler* least_loaded_scheduler()
{
    uint64_t shortestLength = -1;
    Scheduler* leastLoadedScheduler = 0;
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        uint64_t length = (schedulers[i].runningTask != 0);
        for (int64_t priority = TASK_PRIORITY_MAX; priority >= 0; priority--) 
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

    blockedTasks = vector_new(sizeof(BlockedTask));
    nextBalancing = 0;
    schedulerLock = spin_lock_new();

    schedulers = kmalloc(sizeof(Scheduler) * smp_cpu_amount());

    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        schedulers[i].cpu = smp_cpu(i);

        for (uint64_t priority = 0; priority < TASK_PRIORITY_LEVELS; priority++)    
        {
            schedulers[i].queues[priority] = queue_new();
        }
        schedulers[i].runningTask = 0;

        schedulers[i].nextPreemption = 0;
        schedulers[i].lock = spin_lock_new();
        schedulers[i].ticks = 0;
    }

    tty_end_message(TTY_MESSAGE_OK);
}

void scheduler_tick(InterruptFrame* interruptFrame)
{       
    spin_lock_acquire(&schedulerLock);

    if (vector_length(blockedTasks) != 0)
    {
        BlockedTask* blockedTask = vector_get(blockedTasks, 0);

        if (blockedTask->blocker.timeout <= time_nanoseconds())
        {
            scheduler_push(blockedTask->task);
            vector_erase(blockedTasks, 0);
        }
    } 

    if (nextBalancing <= time_nanoseconds())
    {
        for (uint64_t priority = 0; priority < TASK_PRIORITY_LEVELS; priority++)    
        {
            scheduler_balance(priority);
        }

        nextBalancing = time_nanoseconds() + SCHEDULER_BALANCING_PERIOD;
    }

    spin_lock_release(&schedulerLock);

    Ipi ipi = 
    {
        .type = IPI_TYPE_TICK
    };
    smp_send_ipi_to_others(ipi);

    local_scheduler_acquire();
    local_scheduler_tick(interruptFrame);
    local_scheduler_release();
}

void scheduler_balance(uint8_t priority)
{
    scheduler_acquire_all();
    
    uint64_t totalTasks = 0;
    for (int i = 0; i < smp_cpu_amount(); i++)
    {
        totalTasks += queue_length(schedulers[i].queues[priority]) + (schedulers[i].runningTask != 0);
    }

    uint64_t averageLoad = (totalTasks) / smp_cpu_amount();

    for (int j = 0; j < SCHEDULER_BALANCING_ITERATIONS; j++)
    {
        Task* poppedTask = 0;
        for (int i = 0; i < smp_cpu_amount(); i++)
        {
            Queue* queue = schedulers[i].queues[priority];
            uint64_t queueLength = queue_length(queue);

            uint64_t load = (queueLength + (schedulers[i].runningTask != 0));

            if (queueLength != 0 && load > averageLoad && poppedTask == 0)
            {
                poppedTask = queue_pop(queue);
            }
            else if (load <= averageLoad && poppedTask != 0)
            {
                queue_push(queue, poppedTask);
                poppedTask = 0;
            }
        }
    
        if (poppedTask != 0)
        {
            queue_push(schedulers[0].queues[priority], poppedTask);
        }
    }

    scheduler_release_all();
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

void scheduler_emplace(Process* process, InterruptFrame* interruptFrame, uint8_t priority)
{
    if (priority > TASK_PRIORITY_MAX)
    {
        debug_panic("Priority level out of bounds");
    }

    Task* newTask = kmalloc(sizeof(Task));
    newTask->process = process;
    newTask->interruptFrame = interruptFrame;
    newTask->state = TASK_STATE_READY;
    newTask->priority = priority;

    scheduler_push(newTask);
}

void scheduler_push(Task* task)
{
    scheduler_acquire_all();

    Scheduler* scheduler = least_loaded_scheduler();

    if (task->priority + 1 <= TASK_PRIORITY_MAX)
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

void local_scheduler_push(Process* process, InterruptFrame* interruptFrame, uint8_t priority)
{
    if (priority > TASK_PRIORITY_MAX)
    {
        debug_panic("Priority level out of bounds");
    }

    Task* newTask = kmalloc(sizeof(Task));
    newTask->process = process;
    newTask->interruptFrame = interruptFrame;
    newTask->state = TASK_STATE_READY;
    newTask->priority = priority;

    Scheduler* scheduler = scheduler_get_local();

    if (priority + 1 <= TASK_PRIORITY_MAX)
    {
        queue_push(scheduler->queues[priority + 1], newTask);
    }
    else
    {
        queue_push(scheduler->queues[priority], newTask);
    }
}

void local_scheduler_tick(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_get_local();

    scheduler->ticks++;

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

    spin_lock_acquire(&schedulerLock);

    uint8_t taskInserted = 0;
    for (uint64_t i = 0; i < vector_length(blockedTasks); i++)
    {
        BlockedTask* blockedTask = vector_get(blockedTasks, i);
        if (blockedTask->blocker.timeout > newBlockedTask.blocker.timeout)
        {
            vector_insert(blockedTasks, i, &newBlockedTask);
            taskInserted = 1;
            break;
        }
    }

    if (!taskInserted)
    {
        vector_push(blockedTasks, &newBlockedTask);
    }
    
    spin_lock_release(&schedulerLock);
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

Task* local_scheduler_running_task()
{
    return scheduler_get_local()->runningTask;
}