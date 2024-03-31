#include "schedule.h"

#include <string.h>

#include "gdt/gdt.h"
#include "smp/smp.h"
#include "vmm/vmm.h"
#include "heap/heap.h"
#include "sched/sched.h"
#include "debug/debug.h"
#include "regs/regs.h"

static IterResult sched_unblock_iterate(void* element)
{
    Thread* thread = element;

    if (thread->blocker.callback(thread->blocker.context))
    {
        sched_push(thread, 1);
        return ITERATE_ERASE;
    }
    else
    {
        return ITERATE_CONTINUE;
    }
}

static inline Thread* sched_next_thread(Scheduler* scheduler)
{
    if (scheduler->runningThread != NULL && scheduler->runningThread->timeEnd > time_nanoseconds())
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i > scheduler->runningThread->priority + scheduler->runningThread->boost; i--)
        {
            Thread* thread = queue_pop(scheduler->queues[i]);
            if (thread != NULL)
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
            if (thread != NULL)
            {
                return thread;
            }
        }
    }
    
    return NULL;
}

static inline void sched_switch_thread(TrapFrame* trapFrame, Scheduler* scheduler, Thread* next)
{
    Cpu* self = smp_self_unsafe();

    if (next != NULL) //Switch to next thread
    {
        if (scheduler->runningThread != NULL)
        {
            scheduler->runningThread->trapFrame = *trapFrame;    
            queue_push(scheduler->queues[scheduler->runningThread->priority], scheduler->runningThread);
            scheduler->runningThread = NULL;
        }

        next->timeStart = time_nanoseconds();
        next->timeEnd = next->timeStart + SCHED_TIME_SLICE;
        *trapFrame = next->trapFrame;

        space_load(&next->process->space);
        tss_stack_load(&self->tss, (void*)((uint64_t)next->kernelStack + THREAD_KERNEL_STACK_SIZE));

        scheduler->runningThread = next;
    }
    else if (scheduler->runningThread == NULL) //Idle
    {
        memset(trapFrame, 0, sizeof(TrapFrame));
        trapFrame->rip = (uint64_t)sched_idle_loop;
        trapFrame->cs = GDT_KERNEL_CODE;
        trapFrame->ss = GDT_KERNEL_DATA;
        trapFrame->rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
        trapFrame->rsp = (uint64_t)smp_self_unsafe()->idleStack + CPU_IDLE_STACK_SIZE;

        space_load(NULL);
        tss_stack_load(&self->tss, NULL);
    }
    else
    {
        //Keep running the same process
    }
}

void sched_schedule(TrapFrame* trapFrame)
{
    Cpu* self = smp_self();
    Scheduler* scheduler = &self->scheduler;

    if (self->trapDepth != 0)
    {
        smp_put();
        return;
    }

    array_iterate(scheduler->blockedThreads, sched_unblock_iterate);

    while (true)
    {
        Thread* thread = queue_pop(scheduler->killedThreads);
        if (thread == NULL)
        {
            break;
        }

        thread_free(thread);
    }

    if (scheduler->runningThread != NULL)
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
            scheduler->runningThread = NULL;
        }
        break;
        case THREAD_STATE_BLOCKED:
        {
            scheduler->runningThread->trapFrame = *trapFrame;
            array_push(scheduler->blockedThreads, scheduler->runningThread);
            scheduler->runningThread = NULL;
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
    while (true)
    {
        next = sched_next_thread(scheduler);

        //If next has been killed and is in userspace kill next.
        if (next != NULL && next->process->killed && next->trapFrame.cs != GDT_KERNEL_CODE)
        {
            queue_push(scheduler->killedThreads, next);
            next = NULL;
        }
        else
        {
            break;
        }
    }

    sched_switch_thread(trapFrame, scheduler, next);

    smp_put();
}

void sched_push(Thread* thread, uint8_t boost)
{
    int64_t bestLength = INT64_MAX;
    uint64_t best = 0;
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        Scheduler const* scheduler = &smp_cpu(i)->scheduler;

        int64_t length = (int64_t)(scheduler->runningThread != 0);
        for (uint64_t p = THREAD_PRIORITY_MIN; p <= THREAD_PRIORITY_MAX; p++)
        {
            length += queue_length(scheduler->queues[p]);
        }

        if (length == 0) //Bias towards idle cpus
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

    /*if (best != smp_self_unsafe()->id)
    {
        smp_send_ipi(bestCpu, IPI_SCHEDULE);
    }*/
}