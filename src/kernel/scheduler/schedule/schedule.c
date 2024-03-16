#include "schedule.h"

#include "gdt/gdt.h"
#include "smp/smp.h"
#include "vmm/vmm.h"
#include "heap/heap.h"
#include "interrupts/interrupts.h"
#include "scheduler/scheduler.h"
#include "debug/debug.h"
#include "registers/registers.h"

#include <libc/string.h>

static inline void scheduler_clean_graveyard(Scheduler* scheduler)
{
    while (1)
    {
        Thread* thread = queue_pop(scheduler->graveyard);
        if (thread == 0)
        {
            break;
        }

        thread_free(thread);
    }
}

static inline void scheduler_read_state(Scheduler* scheduler)
{
    if (scheduler->runningThread != 0)
    {
        switch (scheduler->runningThread->state)
        {
        case THREAD_STATE_ACTIVE:
        {
            //Do nothing
        }
        break;
        case THREAD_STATE_KILLED:
        {
            if (scheduler->runningThread->id == THREAD_MASTER_ID)
            {
                scheduler->runningThread->process->killed = 1;
            }

            queue_push(scheduler->graveyard, scheduler->runningThread);
            scheduler->runningThread = 0;
        }
        break;
        default:
        {
            debug_panic("Invalid process state");
        }
        break;
        }
    }
}

static inline Thread* scheduler_next_thread(Scheduler* scheduler)
{
    if (scheduler->runningThread != 0 && scheduler->runningThread->timeEnd > time_nanoseconds())
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i > scheduler->runningThread->priority + scheduler->runningThread->boost; i--) 
        {
            Thread* thread = queue_pop(scheduler->queues[i]);
            if (thread != 0)
            {
                return thread;
            }
        }
    }
    else
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i >= THREAD_PRIORITY_MIN; i--) 
        {
            Thread* thread = queue_pop(scheduler->queues[i]);
            if (thread != 0)
            {
                return thread;
            }
        }
    }
    return 0;
}

static inline void scheduler_switch_thread(InterruptFrame* interruptFrame, Scheduler* scheduler, Thread* next)
{
    if (next != 0)
    {    
        Cpu* self = smp_self_unsafe();

        if (scheduler->runningThread != 0)
        {
            interrupt_frame_copy(scheduler->runningThread->interruptFrame, interruptFrame);
            scheduler_push(scheduler->runningThread, 0, scheduler->id);
            scheduler->runningThread = 0;
        }

        next->timeStart = time_nanoseconds();
        next->timeEnd = next->timeStart + SCHEDULER_TIME_SLICE;
        interrupt_frame_copy(interruptFrame, next->interruptFrame);

        address_space_load(next->process->addressSpace);
        self->tss->rsp0 = (uint64_t)next->kernelStackTop;

        scheduler->runningThread = next;
    }
    else if (scheduler->runningThread == 0) //Idle
    {
        Cpu* self = smp_self_unsafe();

        memset(interruptFrame, 0, sizeof(InterruptFrame));
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->flags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
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

    scheduler_clean_graveyard(scheduler);
    scheduler_read_state(scheduler);

    Thread* next;
    while (1)
    {
        next = scheduler_next_thread(scheduler);

        //If next has been killed and is in userspace kill next.
        if (next != 0 && next->process->killed && next->interruptFrame->codeSegment != GDT_KERNEL_CODE)
        {
            queue_push(scheduler->graveyard, next);
            next = 0;
        }
        else
        {
            break;
        }
    }

    scheduler_switch_thread(interruptFrame, scheduler, next);

    scheduler_put();
}

void scheduler_push(Thread* thread, uint8_t boost, uint16_t preferred)
{
    int64_t bestLength = INT64_MAX;
    uint64_t best = preferred < smp_cpu_amount() ? preferred : 0;
    for (int64_t i = smp_cpu_amount() - 1; i >= 0; i--)
    {   
        Scheduler const* scheduler = scheduler_get(i);

        int64_t length = (int64_t)(scheduler->runningThread != 0);
        for (uint64_t p = THREAD_PRIORITY_MIN; p <= THREAD_PRIORITY_MAX; p++) 
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
    
    thread->boost = thread->priority + boost <= THREAD_PRIORITY_MAX ? boost : 0;
    queue_push(scheduler_get(best)->queues[thread->priority + thread->boost], thread);
}