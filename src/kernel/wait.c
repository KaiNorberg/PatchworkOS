#include "wait.h"

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

void wait_thread_ctx_init(wait_thread_ctx_t* wait)
{
    list_init(&wait->entries);
    wait->entryAmount = 0;
    wait->result = WAIT_NORM;
    wait->deadline = 0;
    wait->owner = NULL;
}

static void wait_thread_ctx_acquire_all(wait_thread_ctx_t* wait, wait_queue_t* acquiredQueue)
{
    wait_entry_t* entry;
    LIST_FOR_EACH(entry, &wait->entries, threadEntry)
    {
        if (entry->waitQueue != acquiredQueue)
        {
            lock_acquire(&entry->waitQueue->lock);
        }
    }
}

static void wait_thread_ctx_release_all(wait_thread_ctx_t* wait, wait_queue_t* acquiredQueue)
{
    wait_entry_t* entry;
    LIST_FOR_EACH(entry, &wait->entries, threadEntry)
    {
        if (entry->waitQueue != acquiredQueue)
        {
            lock_release(&entry->waitQueue->lock);
        }
    }
}

static void wait_thread_ctx_release_and_free(wait_thread_ctx_t* wait)
{
    wait_entry_t* temp;
    wait_entry_t* entry;
    LIST_FOR_EACH_SAFE(entry, temp, &wait->entries, threadEntry)
    {
        list_remove(&entry->queueEntry);
        list_remove(&entry->threadEntry);
        lock_release(&entry->waitQueue->lock);
        free(entry);
    }
}

void wait_cpu_ctx_init(wait_cpu_ctx_t* wait)
{
    list_init(&wait->blockedThreads);
    list_init(&wait->parkedThreads);
    lock_init(&wait->lock);
}

static void wait_handle_parked_threads(trap_frame_t* trapFrame, cpu_t* self)
{
    while (1)
    {
        thread_t* thread = LIST_CONTAINER_SAFE(list_pop(&self->wait.parkedThreads), thread_t, entry);
        if (thread == NULL)
        {
            break;
        }

        wait_thread_ctx_acquire_all(&thread->wait, NULL);

        bool shouldUnblock = false;
        wait_entry_t* entry;
        LIST_FOR_EACH(entry, &thread->wait.entries, threadEntry)
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
            thread->wait.result = WAIT_NORM;

            wait_thread_ctx_release_and_free(&thread->wait);

            sched_push(thread);
        }
        else
        {
            thread->wait.owner = self;
            list_push(&self->wait.blockedThreads, &thread->entry);

            wait_thread_ctx_release_all(&thread->wait, NULL);
        }
    }
}

static void wait_handle_blocked_threads(trap_frame_t* trapFrame, cpu_t* self)
{
    // TODO: This is O(n)... fix that
    thread_t* thread;
    thread_t* temp;
    LIST_FOR_EACH_SAFE(thread, temp, &self->wait.blockedThreads, entry)
    {
        wait_result_t result = WAIT_NORM;
        if (thread_dead(thread)) // TODO: Oh so many race conditions.
        {
            result = WAIT_DEAD;
        }
        else if (systime_uptime() >= thread->wait.deadline) // TODO: Sort threads by deadline
        {
            result = WAIT_TIMEOUT;
        }
        else
        {
            continue;
        }

        wait_thread_ctx_acquire_all(&thread->wait, NULL);

        thread->wait.result = result;
        list_remove(&thread->entry);

        wait_thread_ctx_release_and_free(&thread->wait);

        sched_push(thread);
    }
}

void wait_timer_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    LOCK_DEFER(&self->wait.lock);

    wait_handle_parked_threads(trapFrame, self);
    wait_handle_blocked_threads(trapFrame, self);
}

void wait_block_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_ctx_t* sched = &self->sched;
    wait_cpu_ctx_t* cpuCtx = &self->wait;

    thread_t* thread = sched->runThread;
    sched->runThread = NULL;

    thread_save(thread, trapFrame);
    list_push(&cpuCtx->parkedThreads, &thread->entry);

    sched_schedule_trap(trapFrame);
}

void wait_unblock(wait_queue_t* waitQueue, uint64_t amount)
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
        wait_thread_ctx_acquire_all(&thread->wait, waitQueue);

        thread->wait.result = WAIT_NORM;
        lock_acquire(&thread->wait.owner->wait.lock);
        list_remove(&thread->entry);
        lock_release(&thread->wait.owner->wait.lock);

        wait_entry_t* temp2;
        wait_entry_t* entry;
        LIST_FOR_EACH_SAFE(entry, temp2, &thread->wait.entries, threadEntry)
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

// Sets up a threads wait ctx but does not yet block
static uint64_t wait_thread_setup(thread_t* thread, wait_queue_t** waitQueues, uint64_t amount, clock_t timeout)
{
    for (uint64_t i = 0; i < amount; i++)
    {
        wait_entry_t* entry = malloc(sizeof(wait_entry_t));
        if (entry == NULL)
        {
            while (1)
            {
                wait_entry_t* other =
                    LIST_CONTAINER_SAFE(list_pop(&thread->wait.entries), wait_entry_t, threadEntry);
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

        list_push(&thread->wait.entries, &entry->threadEntry);
    }

    thread->wait.entryAmount = amount;
    thread->wait.result = WAIT_NORM;
    thread->wait.deadline = timeout == CLOCKS_NEVER ? CLOCKS_NEVER : systime_uptime() + timeout;
    thread->wait.owner = NULL;

    uint64_t i = 0;
    wait_entry_t* entry;
    LIST_FOR_EACH(entry, &thread->wait.entries, threadEntry)
    {
        list_push(&waitQueues[i]->entries, &entry->queueEntry);
        i++;
    }

    return 0;
}

wait_result_t wait_block(wait_queue_t* waitQueue, clock_t timeout)
{
    if (timeout == 0)
    {
        return WAIT_TIMEOUT;
    }

    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (thread_dead(thread))
    {
        smp_put();
        return WAIT_DEAD;
    }
    if (wait_thread_setup(thread, &waitQueue, 1, timeout) == ERR)
    {
        smp_put();
        return WAIT_ERROR;
    }
    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAIT_BLOCK));

    return thread->wait.result;
}

wait_result_t wait_block_lock(wait_queue_t* waitQueue, clock_t timeout, lock_t* lock)
{
    if (timeout == 0)
    {
        return WAIT_TIMEOUT;
    }

    ASSERT_PANIC(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));
    // Only one lock is allowed to be acquired when calling this function.
    ASSERT_PANIC(smp_self_unsafe()->cli.depth == 1);

    thread_t* thread = smp_self_unsafe()->sched.runThread;
    if (thread_dead(thread))
    {
        return WAIT_DEAD;
    }
    if (wait_thread_setup(thread, &waitQueue, 1, timeout) == ERR)
    {
        return WAIT_ERROR;
    }

    lock_release(lock);
    asm volatile("int %0" ::"i"(VECTOR_WAIT_BLOCK));
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    lock_acquire(lock);

    return thread->wait.result;
}

wait_result_t wait_block_many(wait_queue_t** waitQueues, uint64_t amount, clock_t timeout)
{
    if (timeout == 0)
    {
        return WAIT_TIMEOUT;
    }

    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (thread_dead(thread))
    {
        smp_put();
        return WAIT_DEAD;
    }
    if (wait_thread_setup(thread, waitQueues, amount, timeout) == ERR)
    {
        smp_put();
        return WAIT_ERROR;
    }
    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAIT_BLOCK));

    return thread->wait.result;
}
