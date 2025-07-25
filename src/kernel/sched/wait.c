#include "wait.h"

#include "cpu/smp.h"
#include "drivers/systime/systime.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sched.h"
#include "sched/thread.h"
#include "sync/lock.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <sys/list.h>

void wait_queue_init(wait_queue_t* waitQueue)
{
    lock_init(&waitQueue->lock);
    list_init(&waitQueue->entries);
}

void wait_queue_deinit(wait_queue_t* waitQueue)
{
    LOCK_SCOPE(&waitQueue->lock);

    if (!list_is_empty(&waitQueue->entries))
    {
        panic(NULL, "Wait queue with pending threads freed");
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

typedef enum
{
    WAIT_CLEANUP_NONE = 0,
    WAIT_CLEANUP_ACQUIRE_OWNER = 1 << 0,
    WAIT_CLEANUP_REMOVE_FROM_LIST = 1 << 1,
} wait_cleanup_flags_t;

static void wait_block_cleanup(thread_t* thread, wait_result_t result, wait_queue_t* acquiredQueue,
    wait_cleanup_flags_t flags)
{
    assert(atomic_load(&thread->state) == THREAD_UNBLOCKING);

    thread->wait.result = result;

    if (flags & WAIT_CLEANUP_REMOVE_FROM_LIST)
    {
        if (flags & WAIT_CLEANUP_ACQUIRE_OWNER)
        {
            assert(thread->wait.owner != NULL);
            lock_acquire(&thread->wait.owner->wait.lock);
            list_remove(&thread->entry);
            lock_release(&thread->wait.owner->wait.lock);
        }
        else
        {
            list_remove(&thread->entry);
        }
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
    LOCK_SCOPE(&self->wait.lock);

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
            wait_block_cleanup(thread, WAIT_TIMEOUT, NULL, false);
            sched_push(thread, thread->wait.owner);
        }
    }
}

bool wait_block_finalize(trap_frame_t* trapFrame, cpu_t* self, thread_t* thread)
{
    wait_cpu_ctx_t* cpuCtx = &self->wait;
    LOCK_SCOPE(&self->wait.lock);

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

    thread_state_t expected = THREAD_PRE_BLOCK;
    if (!atomic_compare_exchange_strong(&thread->state, &expected, THREAD_BLOCKED))
    {
        wait_block_cleanup(thread, WAIT_NORM, NULL, WAIT_CLEANUP_REMOVE_FROM_LIST);
        return false;
    }
    else if (thread_is_note_pending(thread))
    {
        thread_state_t expected = THREAD_BLOCKED;
        if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
        {
            wait_block_cleanup(thread, WAIT_NOTE, NULL, WAIT_CLEANUP_REMOVE_FROM_LIST);
            return false;
        }
    }

    systime_timer_one_shot(self, uptime, thread->wait.deadline > uptime ? thread->wait.deadline - uptime : 0);
    return true;
}

void wait_unblock_thread(thread_t* thread, wait_result_t result)
{
    if (atomic_exchange(&thread->state, THREAD_UNBLOCKING) == THREAD_BLOCKED)
    {
        wait_block_cleanup(thread, result, NULL, WAIT_CLEANUP_REMOVE_FROM_LIST | WAIT_CLEANUP_ACQUIRE_OWNER);
        sched_push(thread, thread->wait.owner);
    }
}

uint64_t wait_unblock(wait_queue_t* waitQueue, uint64_t amount)
{
    uint64_t amountUnblocked = 0;

    LOCK_SCOPE(&waitQueue->lock);

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
            wait_block_cleanup(thread, WAIT_NORM, waitQueue,
                WAIT_CLEANUP_REMOVE_FROM_LIST | WAIT_CLEANUP_ACQUIRE_OWNER);
            sched_push(thread, thread->wait.owner);
            amountUnblocked++;
            amount--;
        }
    }

    return amountUnblocked;
}

uint64_t wait_block_setup(wait_queue_t** waitQueues, uint64_t amount, clock_t timeout)
{
    if (waitQueues == NULL || amount == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    // Disable interrupts and retrive thread.
    thread_t* thread = smp_self()->sched.runThread;

    assert(thread != NULL);
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

            for (uint64_t j = 0; j < amount; j++)
            {
                lock_release(&waitQueues[j]->lock);
            }

            smp_put(); // Interrupts enable.
            return ERR;
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

void wait_block_cancel(wait_result_t result)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    thread_t* thread = smp_self_unsafe()->sched.runThread;
    assert(thread != NULL);

    thread_state_t state = atomic_exchange(&thread->state, THREAD_UNBLOCKING);

    // State might already be unblocking if the thread unblocked prematurely.
    assert(state == THREAD_PRE_BLOCK || state == THREAD_UNBLOCKING);

    wait_block_cleanup(thread, result, NULL, WAIT_CLEANUP_NONE);

    thread_state_t newState = atomic_exchange(&thread->state, THREAD_RUNNING);
    assert(newState == THREAD_UNBLOCKING); // Make sure state did not change.

    smp_put(); // Release cpu from wait_block_setup().
    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
}

wait_result_t wait_block_do(void)
{
    thread_t* thread = smp_self_unsafe()->sched.runThread;

    smp_put(); // Release cpu from wait_block_setup().
    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    sched_invoke();
    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
    return thread->wait.result;
}
