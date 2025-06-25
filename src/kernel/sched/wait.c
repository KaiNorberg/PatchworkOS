#include "wait.h"

#include "cpu/regs.h"
#include "cpu/smp.h"
#include "cpu/vectors.h"
#include "drivers/systime/systime.h"
#include "kernel.h"
#include "log/log.h"
#include "mem/heap.h"
#include "sched.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "sys/list.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

void wait_queue_init(wait_queue_t* waitQueue)
{
    lock_init(&waitQueue->lock);
    list_init(&waitQueue->entries);
}

void wait_queue_deinit(wait_queue_t* waitQueue)
{
    LOCK_DEFER(&waitQueue->lock);

    if (!list_is_empty(&waitQueue->entries))
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
    lock_init(&wait->lock);
}

static void wait_cleanup_block(thread_t* thread, wait_result_t result, wait_queue_t* acquiredQueue, bool acquireCpu)
{
    assert(atomic_load(&thread->state) == THREAD_UNBLOCKING);

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
        heap_free(entry);
    }
}

void wait_timer_trap(trap_frame_t* trapFrame, cpu_t* self)
{
    LOCK_DEFER(&self->wait.lock);

    while (1)
    {
        thread_t* thread = CONTAINER_OF_SAFE(list_first(&self->wait.blockedThreads), thread_t, entry);
        if (thread == NULL)
        {
            return;
        }

        clock_t uptime = systime_uptime();
        if (thread->wait.deadline > uptime)
        {
            systime_timer_one_shot(self, uptime, thread->wait.deadline - uptime);
            return;
        }

        list_remove(&thread->entry);

        thread_state_t expected = THREAD_BLOCKED;
        if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
        {
            wait_cleanup_block(thread, WAIT_TIMEOUT, NULL, false);
            sched_push(thread, NULL, thread->wait.owner);
        }
    }
}

bool wait_finalize_block(trap_frame_t* trapFrame, cpu_t* self, thread_t* thread)
{
    wait_cpu_ctx_t* cpuCtx = &self->wait;
    LOCK_DEFER(&self->wait.lock);

    thread->wait.owner = self;

    clock_t uptime = systime_uptime();

    thread_t* lastThread = (CONTAINER_OF(list_last(&self->wait.blockedThreads), thread_t, entry));

    // Sort blocked threads by deadline
    if (thread->wait.deadline == CLOCKS_NEVER || list_is_empty(&self->wait.blockedThreads) ||
        lastThread->wait.deadline <= thread->wait.deadline)
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

    thread_state_t state = atomic_load(&thread->state);
    if (state != THREAD_PRE_BLOCK && state != THREAD_UNBLOCKING)
    {
        LOG_INFO("thread state is THREAD_BLOCKING here state=%d\n", state);
    }

    thread_state_t expected = THREAD_PRE_BLOCK;
    if (!atomic_compare_exchange_strong(&thread->state, &expected, THREAD_BLOCKED))
    {
        wait_cleanup_block(thread, WAIT_NORM, NULL, false);
        return false;
    }
    else if (thread_is_note_pending(thread))
    {
        thread_state_t expected = THREAD_BLOCKED;
        if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
        {
            wait_cleanup_block(thread, WAIT_NOTE, NULL, false);
            return false;
        }
    }

    systime_timer_one_shot(self, uptime, thread->wait.deadline > uptime ? thread->wait.deadline - uptime : 0);
    return true;
}

void wait_unblock_thread(thread_t* thread, wait_result_t result)
{
    wait_cleanup_block(thread, result, NULL, true);
    sched_push(thread, NULL, thread->wait.owner);
}

uint64_t wait_unblock(wait_queue_t* waitQueue, uint64_t amount)
{
    uint64_t amountUnblocked = 0;

    LOCK_DEFER(&waitQueue->lock);

    wait_entry_t* temp;
    wait_entry_t* waitEntry;
    LIST_FOR_EACH_SAFE(waitEntry, temp, &waitQueue->entries, queueEntry)
    {
        if (amount == 0)
        {
            break;
        }

        thread_t* thread = waitEntry->thread;

        if (atomic_exchange(&thread->state, THREAD_UNBLOCKING) == THREAD_BLOCKED)
        {
            wait_cleanup_block(thread, WAIT_NORM, waitQueue, true);
            sched_push(thread, NULL, thread->wait.owner);
            amountUnblocked++;
            amount--;
        }
    }

    return amountUnblocked;
}

// Sets up a threads wait ctx but does not yet block
static uint64_t wait_setup_block(thread_t* thread, wait_queue_t** waitQueues, uint64_t amount, clock_t timeout)
{
    assert(atomic_load(&thread->state) == THREAD_RUNNING);

    for (uint64_t i = 0; i < amount; i++)
    {
        lock_acquire(&waitQueues[i]->lock);
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        wait_entry_t* entry = heap_alloc(sizeof(wait_entry_t), HEAP_NONE);
        if (entry == NULL)
        {
            while (1)
            {
                wait_entry_t* other = CONTAINER_OF_SAFE(list_pop(&thread->wait.entries), wait_entry_t, threadEntry);
                if (other == NULL)
                {
                    break;
                }
                heap_free(other);
            }
            return ERROR(ENOMEM);
        }
        list_entry_init(&entry->queueEntry);
        list_entry_init(&entry->threadEntry);
        entry->waitQueue = waitQueues[i];
        entry->thread = thread;

        list_push(&thread->wait.entries, &entry->threadEntry);
        list_push(&entry->waitQueue->entries, &entry->queueEntry);
    }

    thread->wait.entryAmount = amount;
    thread->wait.result = WAIT_NORM;
    thread->wait.deadline = timeout == CLOCKS_NEVER ? CLOCKS_NEVER : systime_uptime() + timeout;
    thread->wait.owner = NULL;

    for (uint64_t i = 0; i < amount; i++)
    {
        lock_release(&waitQueues[i]->lock);
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
    if (wait_setup_block(thread, &waitQueue, 1, timeout) == ERR)
    {
        smp_put();
        return WAIT_ERROR;
    }
    smp_put();

    sched_invoke();

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
    if (wait_setup_block(thread, &waitQueue, 1, timeout) == ERR)
    {
        return WAIT_ERROR;
    }

    lock_release(lock);
    sched_invoke();
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
    if (wait_setup_block(thread, waitQueues, amount, timeout) == ERR)
    {
        smp_put();
        return WAIT_ERROR;
    }
    smp_put();

    sched_invoke();

    return thread->wait.result;
}
