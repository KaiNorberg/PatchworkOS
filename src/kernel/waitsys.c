#include "waitsys.h"

#include "lock.h"
#include "log.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "sys/list.h"
#include "systime.h"
#include "thread.h"
#include "vectors.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void wait_queue_init(wait_queue_t* waitQueue)
{
    lock_init(&waitQueue->lock);
    list_init(&waitQueue->entries);
}

void wait_queue_deinit(wait_queue_t* waitQueue)
{
    LOCK_DEFER(&waitQueue->lock);

    if (!list_empty(&waitQueue->entries))
    {
        log_panic(NULL, "Wait queue with pending threads freed");
    }
}

void waitsys_thread_ctx_init(waitsys_thread_ctx_t* waitsys)
{
    list_init(&waitsys->entries);
    waitsys->entryAmount = 0;
    waitsys->result = BLOCK_NORM;
    waitsys->deadline = 0;
    waitsys->owner = NULL;
}

static void waitsys_thread_ctx_acquire_all(waitsys_thread_ctx_t* waitsys, wait_queue_t* acquiredQueue)
{
    wait_entry_t* entry;
    LIST_FOR_EACH(entry, &waitsys->entries, threadEntry)
    {
        if (entry->waitQueue != acquiredQueue)
        {
            lock_acquire(&entry->waitQueue->lock);
        }
    }
}

static void waitsys_thread_ctx_release_all(waitsys_thread_ctx_t* waitsys, wait_queue_t* acquiredQueue)
{
    wait_entry_t* entry;
    LIST_FOR_EACH(entry, &waitsys->entries, threadEntry)
    {
        if (entry->waitQueue != acquiredQueue)
        {
            lock_release(&entry->waitQueue->lock);
        }
    }
}

static void waitsys_thread_ctx_release_and_free(waitsys_thread_ctx_t* waitsys)
{
    wait_entry_t* temp;
    wait_entry_t* entry;
    LIST_FOR_EACH_SAFE(entry, temp, &waitsys->entries, threadEntry)
    {
        list_remove(&entry->queueEntry);
        list_remove(&entry->threadEntry);
        lock_release(&entry->waitQueue->lock);
        free(entry);
    }
}

void waitsys_cpu_ctx_init(waitsys_cpu_ctx_t* waitsys)
{
    list_init(&waitsys->blockedThreads);
    list_init(&waitsys->parkedThreads);
    lock_init(&waitsys->lock);
}

static void waitsys_handle_parked_threads(trap_frame_t* trapFrame, cpu_t* self)
{
    while (1)
    {
        thread_t* thread = LIST_CONTAINER_SAFE(list_pop(&self->waitsys.parkedThreads), thread_t, entry);
        if (thread == NULL)
        {
            break;
        }

        waitsys_thread_ctx_acquire_all(&thread->waitsys, NULL);

        bool shouldUnblock = false;
        wait_entry_t* entry;
        LIST_FOR_EACH(entry, &thread->waitsys.entries, threadEntry)
        {
            if (entry->cancelBlock)
            {
                shouldUnblock = true;
                break;
            }
            else
            {
                entry->blocking = true;
            }
        }

        if (shouldUnblock)
        {
            thread->waitsys.result = BLOCK_NORM;

            waitsys_thread_ctx_release_and_free(&thread->waitsys);

            sched_push(thread);
        }
        else
        {
            thread->waitsys.owner = self;
            list_push(&self->waitsys.blockedThreads, &thread->entry);

            waitsys_thread_ctx_release_all(&thread->waitsys, NULL);
        }
    }
}

static void waitsys_handle_blocked_threads(trap_frame_t* trapFrame, cpu_t* self)
{
    // TODO: This is O(n)... fix that
    thread_t* thread;
    thread_t* temp;
    LIST_FOR_EACH_SAFE(thread, temp, &self->waitsys.blockedThreads, entry)
    {
        block_result_t result = BLOCK_NORM;
        if (thread_dead(thread)) // TODO: Oh so many race conditions.
        {
            result = BLOCK_DEAD;
        }
        else if (systime_uptime() >= thread->waitsys.deadline) // TODO: Sort threads by deadline
        {
            result = BLOCK_TIMEOUT;
        }
        else
        {
            continue;
        }

        waitsys_thread_ctx_acquire_all(&thread->waitsys, NULL);

        thread->waitsys.result = result;
        list_remove(&thread->entry);

        waitsys_thread_ctx_release_and_free(&thread->waitsys);

        sched_push(thread);
    }
}

void waitsys_timer_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    LOCK_DEFER(&self->waitsys.lock);

    waitsys_handle_parked_threads(trapFrame, self);
    waitsys_handle_blocked_threads(trapFrame, self);
}

void waitsys_block_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_ctx_t* sched = &self->sched;
    waitsys_cpu_ctx_t* cpuCtx = &self->waitsys;

    thread_t* thread = sched->runThread;
    sched->runThread = NULL;

    thread_save(thread, trapFrame);
    list_push(&cpuCtx->parkedThreads, &thread->entry);

    sched_schedule_trap(trapFrame);
}

void waitsys_unblock(wait_queue_t* waitQueue, uint64_t amount)
{
    LOCK_DEFER(&waitQueue->lock);

    wait_entry_t* temp1;
    wait_entry_t* waitEntry;
    LIST_FOR_EACH_SAFE(waitEntry, temp1, &waitQueue->entries, queueEntry)
    {
        if (amount == 0)
        {
            return;
        }

        if (!waitEntry->blocking)
        {
            waitEntry->cancelBlock = true;
            continue;
        }

        thread_t* thread = waitEntry->thread;
        waitsys_thread_ctx_acquire_all(&thread->waitsys, waitQueue);

        thread->waitsys.result = BLOCK_NORM;
        lock_acquire(&thread->waitsys.owner->waitsys.lock);
        list_remove(&thread->entry);
        lock_release(&thread->waitsys.owner->waitsys.lock);

        wait_entry_t* temp2;
        wait_entry_t* entry;
        LIST_FOR_EACH_SAFE(entry, temp2, &thread->waitsys.entries, threadEntry)
        {
            list_remove(&entry->queueEntry);
            list_remove(&entry->threadEntry);
            if (entry->waitQueue != waitQueue)
            {
                lock_release(&entry->waitQueue->lock);
            }
            free(entry);
        }

        sched_push(thread);
        amount--;
    }
}

// Sets up a threads waitsys ctx but does not yet block
static uint64_t waitsys_thread_setup(thread_t* thread, wait_queue_t** waitQueues, uint64_t amount, clock_t timeout)
{
    for (uint64_t i = 0; i < amount; i++)
    {
        wait_entry_t* entry = malloc(sizeof(wait_entry_t));
        if (entry == NULL)
        {
            while (1)
            {
                wait_entry_t* other =
                    LIST_CONTAINER_SAFE(list_pop(&thread->waitsys.entries), wait_entry_t, threadEntry);
                if (other == NULL)
                {
                    break;
                }
                free(other);
            }
            return ERROR(ENOMEM);
        }
        list_entry_init(&entry->queueEntry);
        list_entry_init(&entry->threadEntry);
        entry->waitQueue = waitQueues[i];
        entry->thread = thread;
        entry->blocking = false;
        entry->cancelBlock = false;

        list_push(&thread->waitsys.entries, &entry->threadEntry);
    }

    thread->waitsys.entryAmount = amount;
    thread->waitsys.result = BLOCK_NORM;
    thread->waitsys.deadline = timeout == CLOCKS_NEVER ? CLOCKS_NEVER : systime_uptime() + timeout;
    thread->waitsys.owner = NULL;

    uint64_t i = 0;
    wait_entry_t* entry;
    LIST_FOR_EACH(entry, &thread->waitsys.entries, threadEntry)
    {
        list_push(&waitQueues[i]->entries, &entry->queueEntry);
        i++;
    }

    return 0;
}

block_result_t waitsys_block(wait_queue_t* waitQueue, clock_t timeout)
{
    if (timeout == 0)
    {
        return BLOCK_TIMEOUT;
    }

    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (thread_dead(thread))
    {
        smp_put();
        return BLOCK_DEAD;
    }
    if (waitsys_thread_setup(thread, &waitQueue, 1, timeout) == ERR)
    {
        smp_put();
        return BLOCK_ERROR;
    }
    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAITSYS_BLOCK));

    return thread->waitsys.result;
}

block_result_t waitsys_block_lock(wait_queue_t* waitQueue, clock_t timeout, lock_t* lock)
{
    if (timeout == 0)
    {
        return BLOCK_TIMEOUT;
    }

    ASSERT_PANIC(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));
    // Only one lock is allowed to be acquired when calling this function.
    ASSERT_PANIC(smp_self_unsafe()->cli.depth == 1);

    thread_t* thread = smp_self_unsafe()->sched.runThread;
    if (thread_dead(thread))
    {
        return BLOCK_DEAD;
    }
    if (waitsys_thread_setup(thread, &waitQueue, 1, timeout) == ERR)
    {
        return BLOCK_ERROR;
    }

    lock_release(lock);
    asm volatile("int %0" ::"i"(VECTOR_WAITSYS_BLOCK));
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    lock_acquire(lock);

    return thread->waitsys.result;
}

block_result_t waitsys_block_many(wait_queue_t** waitQueues, uint64_t amount, clock_t timeout)
{
    if (timeout == 0)
    {
        return BLOCK_TIMEOUT;
    }

    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (thread_dead(thread))
    {
        smp_put();
        return BLOCK_DEAD;
    }
    if (waitsys_thread_setup(thread, waitQueues, amount, timeout) == ERR)
    {
        smp_put();
        return BLOCK_ERROR;
    }
    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAITSYS_BLOCK));

    return thread->waitsys.result;
}
