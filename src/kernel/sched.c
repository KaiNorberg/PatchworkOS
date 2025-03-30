#include "sched.h"

#include "_AUX/ERR.h"
#include "apic.h"
#include "gdt.h"
#include "hpet.h"
#include "loader.h"
#include "lock.h"
#include "log.h"
#include "regs.h"
#include "smp.h"
#include "systime.h"
#include "thread.h"
#include "trap.h"
#include "vectors.h"
#include "vfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

static inline void thread_queue_init(thread_queue_t* queue)
{
    queue->length = 0;
    list_init(&queue->list);
    lock_init(&queue->lock);
}

static inline void thread_queue_push(thread_queue_t* queue, thread_t* thread)
{
    LOCK_DEFER(&queue->lock);

    queue->length++;
    list_push(&queue->list, &thread->entry);
}

static inline thread_t* thread_queue_pop(thread_queue_t* queue)
{
    LOCK_DEFER(&queue->lock);
    if (queue->length == 0)
    {
        return NULL;
    }

    queue->length--;
    return LIST_CONTAINER(list_pop(&queue->list), thread_t, entry);
}

static inline uint64_t thread_queue_length(thread_queue_t* queue)
{
    LOCK_DEFER(&queue->lock);
    return queue->length;
}

void sched_context_init(sched_context_t* context)
{
    for (uint64_t i = PRIORITY_MIN; i <= PRIORITY_MAX; i++)
    {
        thread_queue_init(&context->queues[i]);
    }
    list_init(&context->graveyard);
    context->runThread = NULL;
}

static void sched_context_push(sched_context_t* context, thread_t* thread)
{
    thread_queue_push(&context->queues[thread->priority], thread);
}

static uint64_t sched_context_thread_amount(sched_context_t* context)
{
    uint64_t length = (context->runThread != NULL);
    for (int64_t i = PRIORITY_MAX; i >= PRIORITY_MIN; i--)
    {
        length += thread_queue_length(&context->queues[i]);
    }

    return length;
}

static thread_t* sched_context_find_higher(sched_context_t* context, priority_t priority)
{
    for (int64_t i = PRIORITY_MAX; i > priority; i--)
    {
        thread_t* thread = thread_queue_pop(&context->queues[i]);
        if (thread != NULL)
        {
            if (thread->process->killed && thread->trapFrame.cs != GDT_KERNEL_CODE)
            {
                thread_free(thread);
                return sched_context_find_higher(context, priority);
            }

            return thread;
        }
    }

    return NULL;
}

static thread_t* sched_context_find_any(sched_context_t* context)
{
    for (int64_t i = PRIORITY_MAX; i >= PRIORITY_MIN; i--)
    {
        thread_t* thread = thread_queue_pop(&context->queues[i]);
        if (thread != NULL)
        {
            if (thread->process->killed && thread->trapFrame.cs != GDT_KERNEL_CODE)
            {
                thread_free(thread);
                return sched_context_find_any(context);
            }

            return thread;
        }
    }

    return NULL;
}

static void sched_spawn_boot_thread(void)
{
    thread_t* thread = thread_new(NULL, NULL, PRIORITY_MAX, NULL);
    ASSERT_PANIC(thread != NULL, "failed to create boot thread");
    thread->timeEnd = UINT64_MAX;

    smp_self_unsafe()->sched.runThread = thread;

    printf("sched: spawned boot thread");
}

void sched_init(void)
{
    sched_spawn_boot_thread();
}

block_result_t sched_sleep(nsec_t timeout)
{
    return 0;
}

thread_t* sched_thread(void)
{
    if (!smp_initialized())
    {
        return NULL;
    }

    thread_t* thread = smp_self()->sched.runThread;
    smp_put();
    return thread;
}

process_t* sched_process(void)
{
    if (!smp_initialized())
    {
        return NULL;
    }

    thread_t* thread = sched_thread();
    if (thread == NULL)
    {
        log_panic(NULL, "sched_process called while idle");
    }
    return thread->process;
}

void sched_invoke(void)
{
    asm volatile("int %0" ::"i"(VECTOR_SCHED_SCHEDULE));
}

void sched_yield(void)
{
    thread_t* thread = smp_self()->sched.runThread;
    thread->timeEnd = 0;
    smp_put();

    sched_invoke();
}

void sched_process_exit(uint64_t status)
{
    // TODO: Add handling for status

    sched_context_t* context = &smp_self()->sched;
    context->runThread->killed = true;
    context->runThread->process->killed = true;
    printf("sched: process_exit pid=%d", context->runThread->process->id);
    smp_put();

    sched_invoke();
    log_panic(NULL, "returned from process_exit");
}

void sched_thread_exit(void)
{
    sched_context_t* context = &smp_self()->sched;
    context->runThread->killed = true;
    smp_put();

    sched_invoke();
    log_panic(NULL, "returned from thread_exit");
}

void sched_push(thread_t* thread)
{
    int64_t bestLength = INT64_MAX;
    cpu_t* best = NULL;
    uint64_t cpuAmount = smp_cpu_amount();
    for (uint64_t i = 0; i < cpuAmount; i++)
    {
        cpu_t* cpu = smp_cpu(i);
        sched_context_t* context = &cpu->sched;

        int64_t length = sched_context_thread_amount(context);

        if (length == 0)
        {
            sched_context_push(&cpu->sched, thread);
            return;
        }

        if (length < bestLength)
        {
            bestLength = length;
            best = cpu;
        }
    }

    sched_context_push(&best->sched, thread);
}

static void sched_update_graveyard(trap_frame_t* trapFrame, sched_context_t* context)
{
    while (1)
    {
        thread_t* thread = LIST_CONTAINER_SAFE(list_pop(&context->graveyard), thread_t, entry);
        if (thread == NULL)
        {
            break;
        }

        thread_free(thread);
    }

    if (context->runThread != NULL &&
        (context->runThread->killed || (context->runThread->process->killed && trapFrame->cs != GDT_KERNEL_CODE)))
    {
        list_push(&context->graveyard, &context->runThread->entry);
        context->runThread = NULL;
    }
}

void sched_schedule_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_context_t* context = &self->sched;

    if (self->trapDepth > 1)
    {
        return;
    }

    sched_update_graveyard(trapFrame, context);

    if (context->runThread == NULL)
    {
        thread_t* next = sched_context_find_any(context);
        thread_load(next, trapFrame);
        context->runThread = next;
    }
    else
    {
        thread_t* next = context->runThread->timeEnd < systime_uptime()
            ? sched_context_find_any(context)
            : sched_context_find_higher(context, context->runThread->priority);
        if (next != NULL)
        {
            thread_save(context->runThread, trapFrame);
            sched_context_push(context, context->runThread);

            thread_load(next, trapFrame);
            context->runThread = next;
        }
    }
}