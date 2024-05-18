#include "sched.h"

#include <string.h>

#include "vmm.h"
#include "smp.h"
#include "heap.h"
#include "gdt.h"
#include "time.h"
#include "debug.h"
#include "regs.h"
#include "irq.h"
#include "apic.h"
#include "hpet.h"
#include "loader.h"
#include "tty.h"

static Thread* sched_next_thread(Scheduler* scheduler)
{
    if (scheduler->runningThread != NULL && scheduler->runningThread->timeEnd > time_nanoseconds())
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i > scheduler->runningThread->priority + scheduler->runningThread->boost; i--)
        {
            Thread* thread = queue_pop(&scheduler->queues[i]);
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
            Thread* thread = queue_pop(&scheduler->queues[i]);
            if (thread != NULL)
            {
                return thread;
            }
        }
    }
    
    return NULL;
}

static void sched_switch_thread(TrapFrame* trapFrame, Scheduler* scheduler, Thread* next)
{
    Cpu* self = smp_self_unsafe();

    if (next != NULL) //Switch to next thread
    {
        if (scheduler->runningThread != NULL)
        {
            scheduler->runningThread->trapFrame = *trapFrame;
            scheduler->runningThread->boost = 0;
            queue_push(&scheduler->queues[scheduler->runningThread->priority], scheduler->runningThread);
            scheduler->runningThread = NULL;
        }

        next->timeStart = time_nanoseconds();
        next->timeEnd = next->timeStart + CONFIG_TIME_SLICE;

        *trapFrame = next->trapFrame;
        space_load(&next->process->space);
        tss_stack_load(&self->tss, (void*)((uint64_t)next->kernelStack + CONFIG_KERNEL_STACK));

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

static void sched_spawn_init_thread(void)
{
    Process* process = process_new(NULL);
    Thread* thread = thread_new(process, NULL, THREAD_PRIORITY_MAX);
    thread->timeEnd = UINT64_MAX;

    smp_self_unsafe()->scheduler.runningThread = thread;
}

void scheduler_init(Scheduler* scheduler)
{
    for (uint64_t i = THREAD_PRIORITY_MIN; i <= THREAD_PRIORITY_MAX; i++)
    {
        queue_init(&scheduler->queues[i]);
    }
    list_init(&scheduler->killedThreads);
    list_init(&scheduler->blockedThreads);
    scheduler->runningThread = NULL;
}

void sched_start(void)
{
    tty_start_message("Scheduler starting");

    sched_spawn_init_thread();
    
    smp_send_ipi_to_others(IPI_START);
    SMP_SEND_IPI_TO_SELF(IPI_START);

    tty_end_message(TTY_MESSAGE_OK);
}

void sched_cpu_start(void)
{
    apic_timer_init(IPI_BASE + IPI_SCHEDULE, CONFIG_SCHED_HZ);
}

Thread* sched_thread(void)
{
    Thread* thread = smp_self()->scheduler.runningThread;
    smp_put();
    return thread;
}

Process* sched_process(void)
{
    Process* process = smp_self()->scheduler.runningThread->process;
    smp_put();
    return process;
}

void sched_yield(void)
{
    SMP_SEND_IPI_TO_SELF(IPI_SCHEDULE);
}


void sched_sleep(uint64_t nanoseconds)
{
    Blocker blocker =
    {
        .deadline = time_nanoseconds() + nanoseconds,
    };
    sched_block(blocker);
}

void sched_block(Blocker blocker)
{
    Scheduler* scheduler = &smp_self()->scheduler;
    Thread* thread = scheduler->runningThread;
    thread->blocker = blocker;
    thread->state = THREAD_STATE_BLOCKED;
    smp_put();

    sched_yield();
}

void sched_process_exit(uint64_t status)
{
    //TODO: Add handling for status

    Scheduler* scheduler = &smp_self()->scheduler;
    scheduler->runningThread->state = THREAD_STATE_KILLED;
    scheduler->runningThread->process->killed = true;
    smp_put();

    sched_yield();
    debug_panic("Returned from process_exit");
}

void sched_thread_exit(void)
{
    Scheduler* scheduler = &smp_self()->scheduler;
    scheduler->runningThread->state = THREAD_STATE_KILLED;
    smp_put();

    sched_yield();
    debug_panic("Returned from thread_exit");
}

uint64_t sched_spawn(const char* path)
{
    Process* process = process_new(path);
    Thread* thread = thread_new(process, loader_entry, THREAD_PRIORITY_MIN);
    sched_push(thread, 1);

    return process->id;
}

//Temporary
uint64_t sched_local_thread_amount(void)
{
    Scheduler* scheduler = &smp_self()->scheduler;

    uint64_t length = (scheduler->runningThread != NULL);
    for (uint64_t i = THREAD_PRIORITY_MIN; i <= THREAD_PRIORITY_MAX; i++)
    {
        length += queue_length(&scheduler->queues[i]);
    }

    smp_put();
    return length;
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

    Thread* thread;
    Thread* temp;
    LIST_FOR_EACH_SAFE(thread, temp, &scheduler->blockedThreads)
    {
        if (thread->blocker.deadline < time_nanoseconds() || 
            (thread->blocker.callback != NULL && thread->blocker.callback(thread->blocker.context)))
        {
            list_remove(thread);
            sched_push(thread, 1);
        }
    }

    LIST_FOR_EACH_SAFE(thread, temp, &scheduler->killedThreads)
    {
        list_remove(thread);
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
            list_push(&scheduler->killedThreads, scheduler->runningThread);
            scheduler->runningThread = NULL;
        }
        break;
        case THREAD_STATE_BLOCKED:
        {
            scheduler->runningThread->trapFrame = *trapFrame;
            list_push(&scheduler->blockedThreads, scheduler->runningThread);
            scheduler->runningThread = NULL;
        }
        break;
        default:
        {
            debug_panic("Invalid process state");
        }
        }
    }

    Thread* next;
    while (true)
    {
        next = sched_next_thread(scheduler);

        //If next has been killed and is in userspace kill next.
        if (next != NULL && next->process->killed && next->trapFrame.cs != GDT_KERNEL_CODE)
        {
            list_push(&scheduler->killedThreads, next);
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
    for (int64_t i = smp_cpu_amount() - 1; i >= 0; i--)
    {
        Scheduler* scheduler = &smp_cpu(i)->scheduler;

        int64_t length = (int64_t)(scheduler->runningThread != 0);
        for (uint64_t p = THREAD_PRIORITY_MIN; p <= THREAD_PRIORITY_MAX; p++)
        {
            length += queue_length(&scheduler->queues[p]);
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

    thread->state = THREAD_STATE_ACTIVE;
    thread->boost = thread->priority + boost <= THREAD_PRIORITY_MAX ? boost : 0;
    queue_push(&smp_cpu(best)->scheduler.queues[thread->priority + thread->boost], thread);
}