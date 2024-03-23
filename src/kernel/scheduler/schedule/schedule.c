#include "schedule.h"

#include <string.h>

#include "gdt/gdt.h"
#include "smp/smp.h"
#include "vmm/vmm.h"
#include "heap/heap.h"
#include "interrupts/interrupts.h"
#include "scheduler/scheduler.h"
#include "debug/debug.h"
#include "registers/registers.h"

static uint64_t scheduler_unblock_iterate(void* element)
{
    Thread* thread = element;

    if (thread->process->killed)
    {
        thread_free(thread);

        return ARRAY_ITERATE_ERASE;
    }
    else if (thread->blocker.callback(thread->blocker.context))
    {
        scheduler_push(thread, 1, -1);

        return ARRAY_ITERATE_ERASE;
    }
    else
    {
        return ARRAY_ITERATE_CONTINUE;
    }
}

static inline Thread* scheduler_next_thread(Scheduler* scheduler)
{
    //If thread has not expired look for higher priority thread else look for any thread.
    uint8_t minPriority = scheduler->runningThread != 0 && scheduler->runningThread->timeEnd > time_nanoseconds() ? 
        scheduler->runningThread->priority + scheduler->runningThread->boost : 0;

    for (int64_t i = THREAD_PRIORITY_MAX; i >= minPriority; i--)
    {
        Thread* thread = queue_pop(scheduler->queues[i]);
        if (thread != 0)
        {
            return thread;
        }
    }

    return 0;
}

static inline void scheduler_switch_thread(InterruptFrame* interruptFrame, Scheduler* scheduler, Thread* next)
{
    Cpu* self = smp_self_unsafe();

    if (next != 0) //Switch to next thread
    {
        if (scheduler->runningThread != 0)
        {
            interrupt_frame_copy(&scheduler->runningThread->interruptFrame, interruptFrame);
            scheduler_push(scheduler->runningThread, 0, self->id);
            scheduler->runningThread = 0;
        }

        next->timeStart = time_nanoseconds();
        next->timeEnd = next->timeStart + SCHEDULER_TIME_SLICE;

        interrupt_frame_copy(interruptFrame, &next->interruptFrame);

        address_space_load(next->process->addressSpace);
        tss_stack_load(&self->tss, (void*)((uint64_t)next->kernelStack + THREAD_KERNEL_STACK_SIZE));

        scheduler->runningThread = next;
    }
    else if (scheduler->runningThread == 0) //Idle
    {
        memset(interruptFrame, 0, sizeof(InterruptFrame));
        interruptFrame->instructionPointer = (uint64_t)scheduler_idle_loop;
        interruptFrame->codeSegment = GDT_KERNEL_CODE;
        interruptFrame->stackSegment = GDT_KERNEL_DATA;
        interruptFrame->flags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
        interruptFrame->stackPointer = (uint64_t)smp_self_unsafe()->idleStack + CPU_IDLE_STACK_SIZE;

        address_space_load(0);
        tss_stack_load(&self->tss, 0);
    }
    else
    {
        //Keep running the same process
    }
}

void scheduler_schedule(InterruptFrame* interruptFrame)
{
    Cpu* self = smp_self();
    Scheduler* scheduler = &self->scheduler;

    if (self->interruptDepth != 0)
    {
        smp_put();
        return;
    }

    array_iterate(scheduler->blockedThreads, scheduler_unblock_iterate);

    while (1)
    {
        Thread* thread = queue_pop(scheduler->killedThreads);
        if (thread == 0)
        {
            break;
        }

        thread_free(thread);
    }

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
            queue_push(scheduler->killedThreads, scheduler->runningThread);
            scheduler->runningThread = 0;
        }
        break;
        case THREAD_STATE_BLOCKED:
        {            
            interrupt_frame_copy(&scheduler->runningThread->interruptFrame, interruptFrame);
            array_push(scheduler->blockedThreads, scheduler->runningThread);
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

    Thread* next;
    while (1)
    {
        next = scheduler_next_thread(scheduler);

        //If next has been killed and is in userspace kill next.
        if (next != 0 && next->process->killed && next->interruptFrame.codeSegment != GDT_KERNEL_CODE)
        {
            queue_push(scheduler->killedThreads, next);
            next = 0;
        }
        else
        {
            break;
        }
    }

    scheduler_switch_thread(interruptFrame, scheduler, next);

    smp_put();
}

void scheduler_push(Thread* thread, uint8_t boost, uint16_t preferred)
{
    int64_t bestLength = INT64_MAX;
    uint64_t best = 0;
    for (int64_t i = smp_cpu_amount() - 1; i >= 0; i--)
    {
        Scheduler const* scheduler = &smp_cpu(i)->scheduler;

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

    Cpu const* bestCpu = smp_cpu(best);

    thread->state = THREAD_STATE_ACTIVE;
    thread->boost = thread->priority + boost <= THREAD_PRIORITY_MAX ? boost : 0;
    queue_push(bestCpu->scheduler.queues[thread->priority + thread->boost], thread);

    if (best != smp_self_unsafe()->id)
    {
        smp_send_ipi(bestCpu, IPI_SCHEDULE);
    }
}