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
    return LIST_CONTAINER(list_pop(&queue->list), thread_t, entry);
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

static thread_t* sched_ctx_find_higher(sched_ctx_t* ctx, priority_t priority)
{
    for (int64_t i = PRIORITY_MAX; i > priority; i--)
    {
        thread_t* thread = thread_queue_pop(&ctx->queues[i]);
        if (thread != NULL)
        {
            if (atomic_load(&thread->process->dead) && thread->trapFrame.cs != GDT_KERNEL_CODE)
            {
                thread_free(thread);
                return sched_ctx_find_higher(ctx, priority);
            }

            return thread;
        }
    }

    return NULL;
}

static thread_t* sched_ctx_find_any_same(sched_ctx_t* ctx)
{
    for (int64_t i = PRIORITY_MAX; i >= PRIORITY_MIN; i--)
    {
        thread_t* thread = thread_queue_pop(&ctx->queues[i]);
        if (thread != NULL)
        {
            if (atomic_load(&thread->process->dead) && thread->trapFrame.cs != GDT_KERNEL_CODE)
            {
                thread_free(thread);
                return sched_ctx_find_any_same(ctx);
            }

            return thread;
        }
    }

    return NULL;
}

static thread_t* sched_ctx_find_any(sched_ctx_t* ctx)
{
    thread_t* thread = sched_ctx_find_any_same(ctx);
    if (thread != NULL)
    {
        return thread;
    }

    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        sched_ctx_t* other = &smp_cpu(i)->sched;
        if (ctx == other)
        {
            continue;
        }

        thread_t* thread = sched_ctx_find_any_same(other);
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

    printf("sched: spawned boot thread");
}

void sched_init(void)
{
    sched_spawn_boot_thread();

    wait_queue_init(&sleepQueue);
}

block_result_t sched_sleep(clock_t timeout)
{
    return waitsys_block(&sleepQueue, timeout);
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

void sched_invoke(void)
{
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
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

    sched_ctx_t* ctx = &smp_self()->sched;
    atomic_store(&ctx->runThread->dead, true);
    atomic_store(&ctx->runThread->process->dead, true);
    printf("sched: process_exit pid=%d", ctx->runThread->process->id);
    smp_put();

    sched_invoke();
    log_panic(NULL, "returned from process_exit");
}

void sched_thread_exit(void)
{
    sched_ctx_t* ctx = &smp_self()->sched;
    atomic_store(&ctx->runThread->dead, true);
    smp_put();

    sched_invoke();
    log_panic(NULL, "returned from thread_exit");
}

void sched_push(thread_t* thread)
{
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

    sched_ctx_push(&best->sched, thread);
}

static void sched_update_parked_threads(trap_frame_t* trapFrame, sched_ctx_t* ctx)
{
    while (1)
    {
        thread_t* thread = LIST_CONTAINER_SAFE(list_pop(&ctx->parkedThreads), thread_t, entry);
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
        thread_t* thread = LIST_CONTAINER_SAFE(list_pop(&ctx->zombieThreads), thread_t, entry);
        if (thread == NULL)
        {
            break;
        }

        thread_free(thread);
    }

    if (ctx->runThread != NULL &&
        (atomic_load(&ctx->runThread->dead) ||
            (atomic_load(&ctx->runThread->process->dead) && trapFrame->cs != GDT_KERNEL_CODE)))
    {
        list_push(&ctx->zombieThreads, &ctx->runThread->entry);
        ctx->runThread = NULL;
    }
}

void sched_timer_trap(trap_frame_t* trapFrame)
{
    sched_schedule_trap(trapFrame);
}

void sched_schedule_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_ctx_t* ctx = &self->sched;

    if (self->trapDepth > 1)
    {
        return;
    }

    /*static clock_t lasttime = 0;

    if (self->id == 0 && systime_uptime() > lasttime + CLOCKS_PER_SEC / 10)
    {
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

        uint64_t cpuAmount = smp_cpu_amount();
        for (uint64_t i = 0; i < cpuAmount; i++)
        {
            cpu_t* cpu = smp_cpu(i);

            uint64_t length = sched_ctx_thread_amount(&cpu->sched);
            printf("cpu %d: %.*s>", i, length, "==========================================");
        }
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        lasttime = systime_uptime();
    }*/

    sched_update_parked_threads(trapFrame, ctx);
    sched_update_zombie_threads(trapFrame, ctx);

    if (ctx->runThread == NULL)
    {
        thread_t* next = sched_ctx_find_any(ctx);
        thread_load(next, trapFrame);
        ctx->runThread = next;
    }
    else
    {
        thread_t* next = ctx->runThread->timeEnd < systime_uptime()
            ? sched_ctx_find_any(ctx)
            : sched_ctx_find_higher(ctx, ctx->runThread->priority);
        if (next != NULL)
        {
            thread_save(ctx->runThread, trapFrame);
            list_push(&ctx->parkedThreads, &ctx->runThread->entry);

            thread_load(next, trapFrame);
            ctx->runThread = next;
        }
    }
}
