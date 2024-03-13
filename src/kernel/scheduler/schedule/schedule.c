#include "schedule.h"

#include "gdt/gdt.h"
#include "smp/smp.h"
#include "vmm/vmm.h"
#include "heap/heap.h"
#include "interrupts/interrupts.h"
#include "scheduler/scheduler.h"
#include "debug/debug.h"

#include <libc/string.h>

static inline Process* scheduler_next_process(Scheduler* scheduler)
{
    if (scheduler->runningProcess != 0 && scheduler->runningProcess->timeEnd > time_nanoseconds())
    {
        for (int64_t i = PROCESS_PRIORITY_MAX; i > scheduler->runningProcess->priority + scheduler->runningProcess->boost; i--) 
        {
            Process* process = queue_pop(scheduler->queues[i]);
            if (process != 0)
            {
                return process;
            }
        }
    }
    else
    {
        for (int64_t i = PROCESS_PRIORITY_MAX; i >= PROCESS_PRIORITY_MIN; i--) 
        {
            Process* process = queue_pop(scheduler->queues[i]);
            if (process != 0)
            {
                return process;
            }
        }
    }
    return 0;
}

static inline void scheduler_switch_process(InterruptFrame* interruptFrame, Scheduler* scheduler, Process* next)
{
    if (next != 0)
    {    
        Cpu* self = smp_self_unsafe();

        if (scheduler->runningProcess != 0)
        {
            interrupt_frame_copy(scheduler->runningProcess->interruptFrame, interruptFrame);
            scheduler_push(scheduler->runningProcess, 0, scheduler->id);
            scheduler->runningProcess = 0;
        }

        next->timeStart = time_nanoseconds();
        next->timeEnd = next->timeStart + SCHEDULER_TIME_SLICE;
        interrupt_frame_copy(interruptFrame, next->interruptFrame);

        address_space_load(next->addressSpace);
        self->tss->rsp0 = (uint64_t)next->kernelStackTop;

        scheduler->runningProcess = next;
    }
    else if (scheduler->runningProcess == 0) //Idle
    {    
        Cpu* self = smp_self_unsafe();

        memset(interruptFrame, 0, sizeof(InterruptFrame));
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->flags = 0x202;
        interruptFrame->stackPointer = (uint64_t)self->idleStackTop;

        address_space_load(0);
        self->tss->rsp0 = (uint64_t)self->idleStackTop;
    }
    else
    {
        //Keep running the same process
    }
}

void scheduler_schedule(InterruptFrame* interruptFrame)
{
    Scheduler* scheduler = scheduler_local();

    if (interrupt_depth() != 0)
    {    
        scheduler_put();
        return;
    }

    while (1)
    {
        Process* process = queue_pop(scheduler->graveyard);
        if (process == 0)
        {
            break;
        }

        process_free(process);
    }

    if (scheduler->runningProcess != 0)
    {
        switch (scheduler->runningProcess->state)
        {
        case PROCESS_STATE_ACTIVE:
        {
            //Do nothing
        }
        break;
        case PROCESS_STATE_KILLED:
        {
            queue_push(scheduler->graveyard, scheduler->runningProcess);
            scheduler->runningProcess = 0;
        }
        break;
        default:
        {
            debug_panic("Invalid process state");
        }
        break;
        }
    }

    Process* next = scheduler_next_process(scheduler);
    scheduler_switch_process(interruptFrame, scheduler, next);

    scheduler_put();
}

void scheduler_push(Process* process, uint8_t boost, uint16_t preferred)
{
    int64_t bestLength = INT64_MAX;
    uint64_t best = preferred < smp_cpu_amount() ? preferred : 0;
    for (int64_t i = smp_cpu_amount() - 1; i >= 0; i--)
    {   
        Scheduler const* scheduler = scheduler_get(i);

        int64_t length = (int64_t)(scheduler->runningProcess != 0);
        for (int64_t p = PROCESS_PRIORITY_MAX; p >= PROCESS_PRIORITY_MIN; p--) 
        {
            length += queue_length(scheduler->queues[p]);
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
    
    process->boost = process->priority + boost <= PROCESS_PRIORITY_MAX ? boost : 0;
    queue_push(scheduler_get(best)->queues[process->priority + process->boost], process);
}