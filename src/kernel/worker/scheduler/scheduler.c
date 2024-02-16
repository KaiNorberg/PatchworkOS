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

    scheduler->blockedProcesses = list_new();

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

BlockedProcess* scheduler_block(Scheduler* scheduler, InterruptFrame* interruptFrame, uint64_t timeout)
{
    BlockedProcess* blockedProcess = kmalloc(sizeof(BlockedProcess));
    blockedProcess->process = scheduler->runningProcess;
    blockedProcess->scheduler = scheduler;
    blockedProcess->timeout = timeout;
    blockedProcess->unblock = 0;

    interrupt_frame_copy(scheduler->runningProcess->interruptFrame, interruptFrame);
    scheduler->runningProcess = 0;

    list_push(scheduler->blockedProcesses, blockedProcess);

    return blockedProcess;
}

void scheduler_unblock(Scheduler* scheduler)
{
    ListEntry* entry = scheduler->blockedProcesses->first;
    while (entry != 0)
    {
        BlockedProcess* blockedProcess = entry->data;

        if (blockedProcess->timeout <= time_nanoseconds() || blockedProcess->unblock)
        {
            scheduler_push(scheduler, blockedProcess->process);
            list_erase(scheduler->blockedProcesses, entry);
            kfree(blockedProcess);
            break;
        }

        entry = entry->next;
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
    else //If any task is avilable
    {
        for (int64_t i = PROCESS_PRIORITY_MIN; i <= PROCESS_PRIORITY_MAX; i++) 
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