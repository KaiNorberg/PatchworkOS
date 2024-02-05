#include "scheduler.h"

#include "heap/heap.h"
#include "tty/tty.h"
#include "gdt/gdt.h"
#include "debug/debug.h"
#include "worker_pool/worker_pool.h"

Scheduler* scheduler_new()
{
    Scheduler* scheduler = kmalloc(sizeof(Scheduler));

    for (uint64_t priority = PROCESS_PRIORITY_MIN; priority <= PROCESS_PRIORITY_MAX; priority++)
    {
        scheduler->queues[priority] = queue_new();
    }
    scheduler->runningProcess = 0;

    scheduler->blockedProcesss = vector_new(sizeof(BlockedProcess));

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

void scheduler_push(Scheduler* scheduler, Process* process)
{
    if (process->priority < PROCESS_PRIORITY_MAX)
    {
        queue_push(scheduler->queues[process->priority + 1], process);
    }
    else
    {
        queue_push(scheduler->queues[process->priority], process);
    }
}

void scheduler_exit(Scheduler* scheduler)
{
    process_free(scheduler->runningProcess);
    scheduler->runningProcess = 0;
}

void scheduler_schedule(Scheduler* scheduler, InterruptFrame* interruptFrame)
{        
    Process* newProcess = 0;
    for (int64_t i = PROCESS_PRIORITY_MAX; i >= 0; i--) 
    {
        if (queue_length(scheduler->queues[i]) != 0)
        {
            newProcess = queue_pop(scheduler->queues[i]);
            break;
        }
    }        

    if (newProcess != 0)
    {
        if (scheduler->runningProcess != 0)
        {
            Process* oldProcess = scheduler->runningProcess;
            interrupt_frame_copy(oldProcess->interruptFrame, interruptFrame);

            oldProcess->state = PROCESS_STATE_READY;
            queue_push(scheduler->queues[oldProcess->priority], oldProcess);                
        }

        newProcess->state = PROCESS_STATE_RUNNING;
        scheduler->runningProcess = newProcess;

        interrupt_frame_copy(interruptFrame, newProcess->interruptFrame);

        scheduler->nextPreemption = time_nanoseconds() + SCHEDULER_TIME_SLICE;
    }
    else if (scheduler->runningProcess == 0)
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
    BlockedProcess newBlockedProcess = 
    {
        .blocker = blocker,
        .process = scheduler->runningProcess
    };

    interrupt_frame_copy(scheduler->runningProcess->interruptFrame, interruptFrame);
    scheduler->runningProcess = 0;

    for (uint64_t i = 0; i < vector_length(scheduler->blockedProcesss); i++)
    {
        BlockedProcess const* blockedProcess = vector_get(scheduler->blockedProcesss, i);

        if (blockedProcess->blocker.timeout > newBlockedProcess.blocker.timeout)
        {
            vector_insert(scheduler->blockedProcesss, i, &newBlockedProcess);
            return;
        }
    }
    vector_push_back(scheduler->blockedProcesss, &newBlockedProcess);
}

void scheduler_unblock(Scheduler* scheduler)
{
    if (vector_length(scheduler->blockedProcesss) != 0)
    {
        BlockedProcess* blockedProcess = vector_get(scheduler->blockedProcesss, 0);

        if (blockedProcess->blocker.timeout <= time_nanoseconds())
        {
            scheduler_push(scheduler, blockedProcess->process);
            vector_erase(scheduler->blockedProcesss, 0);
        }
    }
}

uint8_t scheduler_wants_to_schedule(Scheduler const* scheduler)
{
    //If time slice is over and process is available
    if (scheduler->nextPreemption < time_nanoseconds())
    {
        for (int64_t i = PROCESS_PRIORITY_MAX; i >= 0; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                return 1;
            }
        }
    }
    
    //If higher priority process is available
    if (scheduler->runningProcess != 0)
    {
        uint8_t runningPriority = scheduler->runningProcess->priority;
        for (int64_t i = PROCESS_PRIORITY_MAX; i > runningPriority; i--) 
        {
            if (queue_length(scheduler->queues[i]) != 0)
            {
                return 1;
            }
        }
    }

    return 0;
}

uint64_t scheduler_process_amount(Scheduler const* scheduler)
{
    uint64_t processAmount = 0;
    for (uint64_t priority = PROCESS_PRIORITY_MIN; priority <= PROCESS_PRIORITY_MAX; priority++)
    {
        processAmount += queue_length(scheduler->queues[priority]);
    }
    processAmount += scheduler->runningProcess != 0;

    return processAmount;
}