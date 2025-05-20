#include "sched.h"

#include "_AUX/ERR.h"
#include "cpu/apic.h"
#include "cpu/gdt.h"
#include "cpu/regs.h"
#include "cpu/smp.h"
#include "cpu/trap.h"
#include "cpu/vectors.h"
#include "drivers/systime/hpet.h"
#include "drivers/systime/systime.h"
#include "fs/vfs.h"
#include "loader.h"
#include "proc/thread.h"
#include "sync/lock.h"
#include "utils/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

static wait_queue_t sleepQueue;

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
    return CONTAINER_OF(list_pop(&queue->list), thread_t, entry);
}

static inline uint64_t thread_queue_length(thread_queue_t* queue)
{
    LOCK_DEFER(&queue->lock);
    return queue->length;
}

void sched_ctx_init(sched_ctx_t* ctx)
{
    for (uint64_t i = PRIORITY_MIN; i <= PRIORITY_MAX; i++)
    {
        thread_queue_init(&ctx->queues[i]);
    }
    list_init(&ctx->parkedThreads);
    list_init(&ctx->zombieThreads);
    ctx->runThread = NULL;
}

static void sched_ctx_push(sched_ctx_t* ctx, thread_t* thread)
{
    thread_queue_push(&ctx->queues[thread->priority], thread);
}

static uint64_t sched_ctx_thread_amount(sched_ctx_t* ctx)
{
    uint64_t length = (ctx->runThread != NULL);
    for (int64_t i = PRIORITY_MAX; i >= PRIORITY_MIN; i--)
    {
        length += thread_queue_length(&ctx->queues[i]);
    }

    return length;
}

static thread_t* sched_ctx_find(sched_ctx_t* ctx, priority_t minPriority)
{
    for (int64_t i = PRIORITY_MAX; i >= minPriority; i--)
    {
        thread_t* thread = thread_queue_pop(&ctx->queues[i]);
        if (thread != NULL)
        {
            return thread;
        }
    }

    return NULL;
}

static thread_t* sched_find(sched_ctx_t* preferred, priority_t minPriority)
{
    thread_t* thread = sched_ctx_find(preferred, minPriority);
    if (thread != NULL)
    {
        return thread;
    }

    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        sched_ctx_t* other = &smp_cpu(i)->sched;
        if (preferred == other)
        {
            continue;
        }

        thread_t* thread = sched_ctx_find(other, minPriority);
        if (thread != NULL)
        {
            return thread;
        }
    }

    return NULL;
}

static void sched_spawn_boot_thread(void)
{
    process_t* process = process_new(NULL, NULL);
    ASSERT_PANIC_MSG(process != NULL, "failed to create boot process");

    thread_t* thread = thread_new(process, NULL, PRIORITY_MAX);
    ASSERT_PANIC_MSG(thread != NULL, "failed to create boot thread");
    thread->timeEnd = UINT64_MAX;

    smp_self_unsafe()->sched.runThread = thread;

    printf("sched: spawned boot thread\n");
}

void sched_init(void)
{
    printf("sched: init\n");
    sched_spawn_boot_thread();

    wait_queue_init(&sleepQueue);
}

wait_result_t sched_sleep(clock_t timeout)
{
    return wait_block(&sleepQueue, timeout);
}

thread_t* sched_thread(void)
{
    thread_t* thread = smp_self()->sched.runThread;
    smp_put();
    return thread;
}

process_t* sched_process(void)
{
    thread_t* thread = sched_thread();
    if (thread == NULL)
    {
        log_panic(NULL, "sched_process called while idle");
    }
    return thread->process;
}

void sched_process_exit(uint64_t status)
{
    // TODO: Add handling for status, this todo has been here for like literraly a year...
    sched_ctx_t* ctx = &smp_self()->sched;
    thread_t* thread = ctx->runThread;
    process_t* process = thread->process;

    atomic_store(&thread->state, THREAD_ZOMBIE);

    LOCK_DEFER(&process->threads.lock);
    if (process->threads.dying)
    {
        smp_put();
        return;
    }

    process->threads.dying = true;

    thread_t* other;
    LIST_FOR_EACH(other, &process->threads.list, processEntry)
    {
        if (thread == other)
        {
            continue;
        }
        thread_send_note(other, "kill", 4);
    }

    smp_put();
}

void sched_thread_exit(void)
{
    sched_ctx_t* ctx = &smp_self()->sched;
    thread_t* thread = ctx->runThread;
    atomic_store(&thread->state, THREAD_ZOMBIE);
    smp_put();
}

void sched_push(thread_t* thread)
{
    ASSERT_PANIC(atomic_load(&thread->state) == THREAD_PARKED);

    uint64_t cpuAmount = smp_cpu_amount();

    uint64_t bestLength = UINT64_MAX;
    cpu_t* best = NULL;
    for (uint64_t i = 0; i < cpuAmount; i++)
    {
        cpu_t* cpu = smp_cpu(i);

        uint64_t length = sched_ctx_thread_amount(&cpu->sched);
        if (length < bestLength)
        {
            bestLength = length;
            best = cpu;
        }
    }

    atomic_store(&thread->state, THREAD_READY);
    sched_ctx_push(&best->sched, thread);
}

static void sched_update_parked_threads(trap_frame_t* trapFrame, sched_ctx_t* ctx)
{
    while (1)
    {
        thread_t* thread = CONTAINER_OF_SAFE(list_pop(&ctx->parkedThreads), thread_t, entry);
        if (thread == NULL)
        {
            break;
        }

        sched_push(thread);
    }
}

static void sched_update_zombie_threads(trap_frame_t* trapFrame, sched_ctx_t* ctx)
{
    while (1)
    {
        thread_t* thread = CONTAINER_OF_SAFE(list_pop(&ctx->zombieThreads), thread_t, entry);
        if (thread == NULL)
        {
            break;
        }

        thread_free(thread);
    }
}

void sched_timer_trap(trap_frame_t* trapFrame, cpu_t* self)
{
    sched_ctx_t* ctx = &self->sched;

    if (self->trapDepth > 1)
    {
        return;
    }

    sched_update_parked_threads(trapFrame, ctx);
    sched_update_zombie_threads(trapFrame, ctx);
}

void sched_yield(void)
{
    thread_t* thread = smp_self()->sched.runThread;
    thread->timeEnd = 0;
    smp_put();

    sched_invoke();
}

void sched_invoke(void)
{
    asm volatile("int %0" ::"i"(VECTOR_SCHED_INVOKE));
}

void sched_schedule(trap_frame_t* trapFrame, cpu_t* self)
{
    sched_ctx_t* ctx = &self->sched;

    if (self->trapDepth > 1)
    {
        return;
    }

    if (ctx->runThread != NULL)
    {
        thread_state_t state = atomic_load(&ctx->runThread->state);
        if (state == THREAD_ZOMBIE)
        {
            list_push(&ctx->zombieThreads, &ctx->runThread->entry);
            ctx->runThread = NULL;
        }
        else if (state != THREAD_RUNNING)
        {
            return;
        }
    }

    thread_t* next;
    if (ctx->runThread == NULL)
    {
        next = sched_find(ctx, PRIORITY_MIN);
        if (next != NULL)
        {
            atomic_store(&next->state, THREAD_RUNNING);
            thread_load(next, trapFrame);
            ctx->runThread = next;
        }
        else
        {
            thread_load(NULL, trapFrame);
            ctx->runThread = NULL;
        }
    }
    else
    {
        next = sched_find(ctx, ctx->runThread->timeEnd < systime_uptime() ? PRIORITY_MIN : ctx->runThread->priority);
        if (next != NULL)
        {
            atomic_store(&ctx->runThread->state, THREAD_PARKED);
            thread_save(ctx->runThread, trapFrame);
            list_push(&ctx->parkedThreads, &ctx->runThread->entry);
            ctx->runThread = NULL;

            atomic_store(&next->state, THREAD_RUNNING);
            thread_load(next, trapFrame);
            ctx->runThread = next;
        }
    }
}
