#include "wait.h"

#include "cpu/regs.h"
#include "cpu/smp.h"
#include "cpu/vectors.h"
#include "drivers/systime/systime.h"
#include "proc/thread.h"
#include "sched.h"
#include "sync/lock.h"
#include "sys/list.h"
#include "utils/log.h"

#include <assert.h>
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

void wait_cpu_ctx_init(wait_cpu_ctx_t* wait)
{
    list_init(&wait->blockedThreads);
    list_init(&wait->parkedThreads);
    lock_init(&wait->lock);
}

static void wait_handle_parked_threads(trap_frame_t* trapFrame, cpu_t* self)
{
    LOCK_DEFER(&self->wait.lock);

    while (1)
    {
        thread_t* thread = CONTAINER_OF_SAFE(list_pop(&self->wait.parkedThreads), thread_t, entry);
        if (thread == NULL)
        {
            break;
        }

        // Sort blocked threads by deadline
        if (thread->wait.deadline == CLOCKS_NEVER || list_empty(&self->wait.blockedThreads) ||
            CONTAINER_OF(list_last(&self->wait.blockedThreads), thread_t, entry)->wait.deadline <=
                thread->wait.deadline)
        {
            list_push(&self->wait.blockedThreads, &thread->entry);
        }
        else
        {
            thread_t* other;
            LIST_FOR_EACH(other, &self->wait.blockedThreads, entry)
            {
                if (other->wait.deadline > thread->wait.deadline)
                {
                    list_prepend(&other->entry, &thread->entry);
                    break;
                }
            }
        }

        thread_state_t expected = THREAD_PRE_BLOCK;
        if (!atomic_compare_exchange_strong(&thread->state, &expected, THREAD_BLOCKED))
        {
            wait_unblock_thread(thread, WAIT_NORM, NULL, false);
        }
        else if (thread_note_pending(thread))
        {
            thread_state_t expected = THREAD_BLOCKED;
            if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
            {
                wait_unblock_thread(thread, WAIT_NOTE, NULL, false);
            }
        }
    }
}

static void wait_handle_blocked_threads(trap_frame_t* trapFrame, cpu_t* self)
{
    LOCK_DEFER(&self->wait.lock);

    thread_t* thread = CONTAINER_OF_SAFE(list_first(&self->wait.blockedThreads), thread_t, entry);
    if (thread == NULL)
    {
        return;
    }

    if (systime_uptime() < thread->wait.deadline)
    {
        return;
    }

    list_remove(&thread->entry);

    _Atomic(thread_state_t) expected = ATOMIC_VAR_INIT(THREAD_BLOCKED);
    if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
    {
        wait_unblock_thread(thread, WAIT_TIMEOUT, NULL, false);
    }
}

void wait_timer_trap(trap_frame_t* trapFrame, cpu_t* self)
{
    wait_handle_parked_threads(trapFrame, self);
    wait_handle_blocked_threads(trapFrame, self);
}

void wait_block_trap(trap_frame_t* trapFrame, cpu_t* self)
{
    sched_ctx_t* sched = &self->sched;
    wait_cpu_ctx_t* cpuCtx = &self->wait;

    thread_t* thread = sched->runThread;
    sched->runThread = NULL;

    thread_save(thread, trapFrame);

    lock_acquire(&cpuCtx->lock);
    thread->wait.owner = self;
    list_push(&cpuCtx->parkedThreads, &thread->entry);
    lock_release(&cpuCtx->lock);

    sched_schedule(trapFrame, self);
}

void wait_unblock_thread(thread_t* thread, wait_result_t result, wait_queue_t* acquiredQueue, bool acquireCpu)
{
    thread_state_t state = atomic_load(&thread->state);
    assert(state == THREAD_UNBLOCKING);

    thread->wait.result = result;
    if (acquireCpu)
    {
        lock_acquire(&thread->wait.owner->wait.lock);
        list_remove(&thread->entry);
        lock_release(&thread->wait.owner->wait.lock);
    }
    else
    {
        list_remove(&thread->entry);
    }

    wait_entry_t* temp;
    wait_entry_t* entry;
    LIST_FOR_EACH_SAFE(entry, temp, &thread->wait.entries, threadEntry)
    {
        if (entry->waitQueue != acquiredQueue)
        {
            lock_acquire(&entry->waitQueue->lock);
        }
        list_remove(&entry->queueEntry);
        list_remove(&entry->threadEntry);
        if (entry->waitQueue != acquiredQueue)
        {
            lock_release(&entry->waitQueue->lock);
        }
        free(entry);
    }

    thread_state_t expected = THREAD_UNBLOCKING;
    atomic_compare_exchange_strong(&thread->state, &expected, THREAD_PARKED);
    sched_push(thread);
}

void wait_unblock(wait_queue_t* waitQueue, uint64_t amount)
{
    LOCK_DEFER(&waitQueue->lock);

    wait_entry_t* temp;
    wait_entry_t* waitEntry;
    LIST_FOR_EACH_SAFE(waitEntry, temp, &waitQueue->entries, queueEntry)
    {
        if (amount == 0)
        {
            return;
        }

        thread_t* thread = waitEntry->thread;

        if (atomic_exchange(&thread->state, THREAD_UNBLOCKING) == THREAD_BLOCKED)
        {
            wait_unblock_thread(thread, WAIT_NORM, waitQueue, true);
            amount--;
        }
    }
}

// Sets up a threads wait ctx but does not yet block
static uint64_t wait_thread_setup(thread_t* thread, wait_queue_t** waitQueues, uint64_t amount, clock_t timeout)
{
    assert(atomic_load(&thread->state) == THREAD_RUNNING);

    for (uint64_t i = 0; i < amount; i++)
    {
        wait_entry_t* entry = malloc(sizeof(wait_entry_t));
        if (entry == NULL)
        {
            while (1)
            {
                wait_entry_t* other = CONTAINER_OF_SAFE(list_pop(&thread->wait.entries), wait_entry_t, threadEntry);
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
        lock_acquire(&waitQueues[i]->lock);
        list_push(&waitQueues[i]->entries, &entry->queueEntry);
        lock_release(&waitQueues[i]->lock);
        i++;
    }

    atomic_store(&thread->state, THREAD_PRE_BLOCK);
    return 0;
}

wait_result_t wait_block(wait_queue_t* waitQueue, clock_t timeout)
{
    if (timeout == 0)
    {
        return WAIT_TIMEOUT;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
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

    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));
    // Only one lock is allowed to be acquired when calling this function.
    assert(smp_self_unsafe()->cli.depth == 1);

    thread_t* thread = smp_self_unsafe()->sched.runThread;
    if (wait_thread_setup(thread, &waitQueue, 1, timeout) == ERR)
    {
        return WAIT_ERROR;
    }

    lock_release(lock);
    asm volatile("int %0" ::"i"(VECTOR_WAIT_BLOCK));
    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    lock_acquire(lock);

    return thread->wait.result;
}

wait_result_t wait_block_many(wait_queue_t** waitQueues, uint64_t amount, clock_t timeout)
{
    if (timeout == 0)
    {
        return WAIT_TIMEOUT;
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (wait_thread_setup(thread, waitQueues, amount, timeout) == ERR)
    {
        smp_put();
        return WAIT_ERROR;
    }
    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAIT_BLOCK));

    return thread->wait.result;
}