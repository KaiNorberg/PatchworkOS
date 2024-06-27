#include "sched.h"

#include "apic.h"
#include "gdt.h"
#include "list.h"
#include "loader.h"
#include "log.h"
#include "smp.h"
#include "time.h"

#include <stdlib.h>
#include <string.h>

static void sched_push(thread_t* thread)
{
    int64_t bestLength = INT64_MAX;
    uint64_t best = 0;
    for (int64_t i = smp_cpu_amount() - 1; i >= 0; i--)
    {
        const sched_context_t* context = &smp_cpu(i)->schedContext;

        int64_t length = (int64_t)(context->runningThread != 0);
        for (uint64_t p = THREAD_PRIORITY_MIN; p <= THREAD_PRIORITY_MAX; p++)
        {
            length += queue_length(&context->queues[p]);
        }

        if (length == 0) // Bias towards idle cpus
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
    queue_push(&smp_cpu(best)->schedContext.queues[thread->priority], thread);
}

static thread_t* sched_next_thread(sched_context_t* context)
{
    if (context->runningThread != NULL && context->runningThread->timeEnd > time_uptime())
    {
        for (int64_t i = THREAD_PRIORITY_MAX; i > context->runningThread->priority; i--)
        {
            thread_t* thread = queue_pop(&context->queues[i]);
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
            thread_t* thread = queue_pop(&context->queues[i]);
            if (thread != NULL)
            {
                return thread;
            }
        }
    }

    return NULL;
}

static void sched_switch_thread(trap_frame_t* trapFrame, sched_context_t* context, thread_t* next)
{
    if (context->runningThread == NULL) // Load next thread or idle if (next == NULL)
    {
        thread_load(next, trapFrame);
        context->runningThread = next;
    }
    else
    {
        if (next != NULL) // Switch to next
        {
            thread_save(context->runningThread, trapFrame);
            queue_push(&context->queues[context->runningThread->priority], context->runningThread);
            context->runningThread = NULL;

            thread_load(next, trapFrame);
            context->runningThread = next;
        }
        else if (context->runningThread->state == THREAD_STATE_PAUSE) // Pause
        {
            context->runningThread->state = THREAD_STATE_ACTIVE;

            thread_save(context->runningThread, trapFrame);
            queue_push(&context->queues[context->runningThread->priority], context->runningThread);
            context->runningThread = NULL;

            thread_load(NULL, trapFrame);
        }
        else
        {
            // Keep running the same thread
        }
    }
}

static void sched_spawn_init_thread(void)
{
    process_t* process = process_new(NULL);
    thread_t* thread = thread_new(process, NULL, THREAD_PRIORITY_MAX);
    thread->timeEnd = UINT64_MAX;

    smp_self_unsafe()->schedContext.runningThread = thread;
}

void sched_context_init(sched_context_t* context)
{
    for (uint64_t i = THREAD_PRIORITY_MIN; i <= THREAD_PRIORITY_MAX; i++)
    {
        queue_init(&context->queues[i]);
    }
    list_init(&context->graveyard);
    context->runningThread = NULL;
}

void sched_start(void)
{
    log_print("Starting scheduler");

    sched_spawn_init_thread();

    smp_send_ipi_to_others(IPI_START);
    SMP_SEND_IPI_TO_SELF(IPI_START);
}

void sched_cpu_start(void)
{
    apic_timer_init(IPI_BASE + IPI_SCHEDULE, CONFIG_SCHED_HZ);
}

thread_t* sched_thread(void)
{
    thread_t* thread = smp_self()->schedContext.runningThread;
    smp_put();
    return thread;
}

process_t* sched_process(void)
{
    process_t* process = smp_self()->schedContext.runningThread->process;
    smp_put();
    return process;
}

void sched_yield(void)
{
    thread_t* thread = sched_thread();
    thread->timeEnd = 0;
    SMP_SEND_IPI_TO_SELF(IPI_SCHEDULE);
}

void sched_pause(void)
{
    thread_t* thread = smp_self()->schedContext.runningThread;
    thread->timeEnd = 0;
    thread->state = THREAD_STATE_PAUSE;
    smp_put();
    SMP_SEND_IPI_TO_SELF(IPI_SCHEDULE);
}

void sched_process_exit(uint64_t status)
{
    // TODO: Add handling for status

    sched_context_t* context = &smp_self()->schedContext;
    context->runningThread->state = THREAD_STATE_KILLED;
    context->runningThread->process->killed = true;
    smp_put();

    sched_yield();
    log_panic(NULL, "returned from process_exit");
}

void sched_thread_exit(void)
{
    sched_context_t* context = &smp_self()->schedContext;
    context->runningThread->state = THREAD_STATE_KILLED;
    smp_put();

    sched_yield();
    log_panic(NULL, "returned from thread_exit");
}

pid_t sched_spawn(const char* path, uint8_t priority)
{
    process_t* process = process_new(path);
    if (process == NULL)
    {
        return ERR;
    }

    thread_t* thread = thread_new(process, loader_entry, priority);
    sched_push(thread);

    return process->id;
}

tid_t sched_thread_spawn(void* entry, uint8_t priority)
{
    thread_t* thread = thread_new(sched_process(), entry, priority);
    sched_push(thread);

    return thread->id;
}

uint64_t sched_local_thread_amount(void)
{
    const sched_context_t* context = &smp_self()->schedContext;

    uint64_t length = (context->runningThread != NULL);
    for (uint64_t i = THREAD_PRIORITY_MIN; i <= THREAD_PRIORITY_MAX; i++)
    {
        length += queue_length(&context->queues[i]);
    }

    smp_put();
    return length;
}

void sched_schedule(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self();
    sched_context_t* context = &self->schedContext;

    if (self->trapDepth != 0)
    {
        smp_put();
        return;
    }

    while (1)
    {
        thread_t* thread = list_pop(&context->graveyard);
        if (thread == NULL)
        {
            break;
        }

        thread_free(thread);
    }

    thread_state_t state = context->runningThread != NULL ? context->runningThread->state : THREAD_STATE_NONE;
    if (state == THREAD_STATE_KILLED)
    {
        list_push(&context->graveyard, context->runningThread);
        context->runningThread = NULL;
    }

    thread_t* next;
    while (true)
    {
        next = sched_next_thread(context);

        // If next has been killed and is in userspace kill next.
        if (next != NULL && next->process->killed && next->trapFrame.cs != GDT_KERNEL_CODE)
        {
            list_push(&context->graveyard, next);
            next = NULL;
        }
        else
        {
            break;
        }
    }

    sched_switch_thread(trapFrame, context, next);

    smp_put();
}
