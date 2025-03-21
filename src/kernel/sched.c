#include "sched.h"

#include "_AUX/ERR.h"
#include "apic.h"
#include "gdt.h"
#include "hpet.h"
#include "loader.h"
#include "lock.h"
#include "log.h"
#include "queue.h"
#include "regs.h"
#include "smp.h"
#include "thread.h"
#include "time.h"
#include "trap.h"
#include "vectors.h"
#include "vfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

static list_t blockers;
static lock_t blockersLock;

static blocker_t sleepBlocker;

void blocker_init(blocker_t* blocker)
{
    list_entry_init(&blocker->entry);
    list_init(&blocker->threads);
    lock_init(&blocker->lock);

    LOCK_DEFER(&blockersLock);
    list_push(&blockers, blocker);
}

void blocker_deinit(blocker_t* blocker)
{
    lock_acquire(&blocker->lock);

    if (!list_empty(&blocker->threads))
    {
        log_panic(NULL, "Blocker with pending threads freed");
    }
    lock_release(&blocker->lock);

    LOCK_DEFER(&blockersLock);
    list_remove(blocker);
}

static void blocker_push(blocker_t* blocker, thread_t* thread)
{
    LOCK_DEFER(&blocker->lock);

    thread_t* other;
    LIST_FOR_EACH(other, &blocker->threads)
    {
        if (other->block.deadline > thread->block.deadline)
        {
            list_prepend(&other->entry, thread);
            return;
        }
    }

    list_push(&blocker->threads, thread);
}

void sched_context_init(sched_context_t* context)
{
    for (uint64_t i = PRIORITY_MIN; i <= PRIORITY_MAX; i++)
    {
        queue_init(&context->queues[i]);
    }
    list_init(&context->graveyard);
    context->runThread = NULL;
}

static void sched_context_push(sched_context_t* context, thread_t* thread)
{
    queue_push(&context->queues[thread->priority], thread);
}

static uint64_t sched_context_thread_amount(const sched_context_t* context)
{
    uint64_t length = (context->runThread != NULL);
    for (int64_t i = PRIORITY_MAX; i >= PRIORITY_MIN; i--)
    {
        length += queue_length(&context->queues[i]);
    }

    return length;
}

static thread_t* sched_context_find_higher(sched_context_t* context, priority_t priority)
{
    for (int64_t i = PRIORITY_MAX; i > priority; i--)
    {
        thread_t* thread = queue_pop(&context->queues[i]);
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
        thread_t* thread = queue_pop(&context->queues[i]);
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

static void sched_spawn_init_thread(void)
{
    thread_t* thread = thread_new(NULL, NULL, PRIORITY_MAX);
    ASSERT_PANIC(thread != NULL, "failed to create init thread");
    thread->timeEnd = UINT64_MAX;

    smp_self_unsafe()->sched.runThread = thread;
}

void sched_init(void)
{
    list_init(&blockers);
    lock_init(&blockersLock);

    blocker_init(&sleepBlocker);

    sched_spawn_init_thread();

    printf("sched: init");
}

static void sched_start_ipi(trap_frame_t* trapFrame)
{
    nsec_t uptime = time_uptime();
    nsec_t interval = (SEC / CONFIG_SCHED_HZ) / smp_cpu_amount();
    nsec_t offset = ROUND_UP(uptime, interval) - uptime;
    hpet_sleep(offset + interval * smp_self_unsafe()->id);

    apic_timer_init(VECTOR_SCHED_TIMER, CONFIG_SCHED_HZ);
}

void sched_start(void)
{
    smp_send_others(sched_start_ipi);
    smp_send_self(sched_start_ipi);

    printf("sched: start");
}

block_result_t sched_sleep(nsec_t timeout)
{
    return sched_block(&sleepBlocker, timeout);
}

block_result_t sched_block(blocker_t* blocker, nsec_t timeout)
{
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE, "sched_block, interupts disabled");

    thread_t* thread = smp_self()->sched.runThread;
    thread->timeEnd = 0;
    thread->block.deadline = timeout == NEVER ? NEVER : timeout + time_uptime();
    thread->block.blocker = blocker;
    smp_put();

    sched_invoke();
    return thread->block.result;
}

void sched_unblock(blocker_t* blocker)
{
    LOCK_DEFER(&blocker->lock);

    while (1)
    {
        thread_t* thread = list_pop(&blocker->threads);
        if (thread == NULL)
        {
            break;
        }

        thread->block.deadline = 0;
        thread->block.result = BLOCK_NORM;
        thread->block.blocker = NULL;
        sched_push(thread);
    }
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
    asm volatile("int %0" ::"i"(VECTOR_SCHED_INVOKE));
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
    printf("sched: process exit (%d)", context->runThread->process->id);
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
        const sched_context_t* context = &cpu->sched;

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

static void sched_update_blockers(void)
{
    LOCK_DEFER(&blockersLock);
    nsec_t uptime = time_uptime();

    blocker_t* blocker;
    LIST_FOR_EACH(blocker, &blockers)
    {
        LOCK_DEFER(&blocker->lock);

        thread_t* thread = list_first(&blocker->threads);
        if (thread != NULL && thread->block.deadline < uptime)
        {
            thread->block.result = BLOCK_TIMEOUT;
            thread->block.blocker = NULL;
            list_remove(thread);
            sched_push(thread);
        }
    }
}

static void sched_update_graveyard(trap_frame_t* trapFrame, sched_context_t* context)
{
    while (1)
    {
        thread_t* thread = list_pop(&context->graveyard);
        if (thread == NULL)
        {
            break;
        }

        thread_free(thread);
    }

    if (context->runThread != NULL &&
        (context->runThread->killed || (context->runThread->process->killed && trapFrame->cs != GDT_KERNEL_CODE)))
    {
        list_push(&context->graveyard, context->runThread);
        context->runThread = NULL;
    }
}

void sched_schedule(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_context_t* context = &self->sched;

    if (self->trapDepth > 1)
    {
        return;
    }

    if (self->id == 0)
    {
        sched_update_blockers();
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
        blocker_t* blocker = context->runThread->block.blocker;
        if (blocker != NULL)
        {
            thread_save(context->runThread, trapFrame);
            blocker_push(blocker, context->runThread);

            thread_t* next = sched_context_find_any(context);
            thread_load(next, trapFrame);
            context->runThread = next;
        }
        else
        {
            thread_t* next = context->runThread->timeEnd < time_uptime()
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
}
