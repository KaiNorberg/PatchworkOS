#include <kernel/sched/wait.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/smp.h>
#include <kernel/log/panic.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/list.h>

static void wait_remove_wait_entries(thread_t* thread, errno_t err)
{
    assert(atomic_load(&thread->state) == THREAD_UNBLOCKING);

    thread->wait.err = err;

    wait_entry_t* temp;
    wait_entry_t* entry;
    LIST_FOR_EACH_SAFE(entry, temp, &thread->wait.entries, threadEntry)
    {
        lock_acquire(&entry->waitQueue->lock);
        list_remove(&entry->waitQueue->entries, &entry->queueEntry);
        lock_release(&entry->waitQueue->lock);

        list_remove(&entry->thread->wait.entries, &entry->threadEntry); // Belongs to thread, no lock needed.
        free(entry);
    }
}

static void wait_timer_handler(interrupt_frame_t* frame, cpu_t* self)
{
    (void)frame; // Unused

    LOCK_SCOPE(&self->wait.lock);

    while (1)
    {
        thread_t* thread = CONTAINER_OF_SAFE(list_first(&self->wait.blockedThreads), thread_t, entry);
        if (thread == NULL)
        {
            return;
        }

        clock_t uptime = sys_time_uptime();
        if (thread->wait.deadline > uptime)
        {
            timer_one_shot(self, uptime, thread->wait.deadline - uptime);
            return;
        }

        list_remove(&self->wait.blockedThreads, &thread->entry);

        // Already unblocking.
        thread_state_t expected = THREAD_BLOCKED;
        if (!atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
        {
            continue;
        }

        wait_remove_wait_entries(thread, ETIMEDOUT);
        sched_push(thread, thread->wait.cpu->cpu);
    }
}

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
    wait->err = EOK;
    wait->deadline = 0;
    wait->cpu = NULL;
}

void wait_cpu_ctx_init(wait_cpu_ctx_t* wait, cpu_t* self)
{
    list_init(&wait->blockedThreads);
    wait->cpu = self;
    lock_init(&wait->lock);
    timer_register_callback(&self->timer, wait_timer_handler);
}

bool wait_block_finalize(interrupt_frame_t* frame, cpu_t* self, thread_t* thread, clock_t uptime)
{
    (void)frame; // Unused

    thread->wait.cpu = &self->wait;
    LOCK_SCOPE(&self->wait.lock);

    thread_t* lastThread = (CONTAINER_OF(list_last(&self->wait.blockedThreads), thread_t, entry));

    // Sort blocked threads by deadline
    if (thread->wait.deadline == CLOCKS_NEVER || list_is_empty(&self->wait.blockedThreads) ||
        lastThread->wait.deadline <= thread->wait.deadline)
    {
        list_push_back(&self->wait.blockedThreads, &thread->entry);
    }
    else
    {
        thread_t* other;
        LIST_FOR_EACH(other, &self->wait.blockedThreads, entry)
        {
            if (other->wait.deadline > thread->wait.deadline)
            {
                list_prepend(&self->wait.blockedThreads, &other->entry, &thread->entry);
                break;
            }
        }
    }

    thread_state_t expected = THREAD_PRE_BLOCK;
    if (!atomic_compare_exchange_strong(&thread->state, &expected, THREAD_BLOCKED)) // Prematurely unblocked
    {
        list_remove(&self->wait.blockedThreads, &thread->entry);
        wait_remove_wait_entries(thread, EOK);
        atomic_store(&thread->state, THREAD_RUNNING);
        return false;
    }

    if (thread_is_note_pending(thread))
    {
        thread_state_t expected = THREAD_BLOCKED;
        if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
        {
            list_remove(&self->wait.blockedThreads, &thread->entry);
            wait_remove_wait_entries(thread, EINTR);
            atomic_store(&thread->state, THREAD_RUNNING);
            return false;
        }
    }

    timer_one_shot(self, uptime, thread->wait.deadline > uptime ? thread->wait.deadline - uptime : 0);
    return true;
}

void wait_unblock_thread(thread_t* thread, errno_t err)
{
    assert(atomic_load(&thread->state) == THREAD_UNBLOCKING);

    lock_acquire(&thread->wait.cpu->lock);
    list_remove(&thread->wait.cpu->blockedThreads, &thread->entry);
    wait_remove_wait_entries(thread, err);
    lock_release(&thread->wait.cpu->lock);

    sched_push(thread, thread->wait.cpu->cpu);
}

uint64_t wait_unblock(wait_queue_t* waitQueue, uint64_t amount, errno_t err)
{
    uint64_t amountUnblocked = 0;

    const uint64_t threadsPerBatch = 64;
    while (1)
    {
        // For concistent lock ordering we first remove from the wait queue, release the wait queues lock and then
        // acquire the threads cpu lock. Such that every time we acquire the locks its, cpu lock -> wait queue lock.

        thread_t* threads[threadsPerBatch];
        uint64_t toUnblock = amount < threadsPerBatch ? amount : threadsPerBatch;

        lock_acquire(&waitQueue->lock);

        wait_entry_t* temp;
        wait_entry_t* waitEntry;
        uint64_t collected = 0;
        LIST_FOR_EACH_SAFE(waitEntry, temp, &waitQueue->entries, queueEntry)
        {
            if (collected == toUnblock)
            {
                break;
            }

            thread_t* thread = waitEntry->thread;

            if (atomic_exchange(&thread->state, THREAD_UNBLOCKING) == THREAD_BLOCKED)
            {
                list_remove(&waitQueue->entries, &waitEntry->queueEntry);
                list_remove(&thread->wait.entries, &waitEntry->threadEntry);
                free(waitEntry);
                threads[collected] = thread;
                collected++;
            }
        }

        lock_release(&waitQueue->lock);

        if (collected == 0)
        {
            break;
        }

        for (uint64_t i = 0; i < collected; i++)
        {
            lock_acquire(&threads[i]->wait.cpu->lock);
            list_remove(&threads[i]->wait.cpu->blockedThreads, &threads[i]->entry);
            wait_remove_wait_entries(threads[i], err);
            lock_release(&threads[i]->wait.cpu->lock);

            sched_push(threads[i], threads[i]->wait.cpu->cpu);
            amountUnblocked++;
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

    for (uint64_t i = 0; i < amount; i++)
    {
        lock_acquire(&waitQueues[i]->lock);
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        wait_entry_t* entry = malloc(sizeof(wait_entry_t));
        if (entry == NULL)
        {
            while (1)
            {
                wait_entry_t* other =
                    CONTAINER_OF_SAFE(list_pop_first(&thread->wait.entries), wait_entry_t, threadEntry);
                if (other == NULL)
                {
                    break;
                }
                free(other);
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

        list_push_back(&thread->wait.entries, &entry->threadEntry);
        list_push_back(&entry->waitQueue->entries, &entry->queueEntry);
    }

    thread->wait.err = EOK;
    thread->wait.deadline = timeout == CLOCKS_NEVER ? CLOCKS_NEVER : sys_time_uptime() + timeout;
    thread->wait.cpu = NULL;

    for (uint64_t i = 0; i < amount; i++)
    {
        lock_release(&waitQueues[i]->lock);
    }

    thread_state_t expected = THREAD_RUNNING;
    if (!atomic_compare_exchange_strong(&thread->state, &expected, THREAD_PRE_BLOCK))
    {
        if (expected != THREAD_UNBLOCKING)
        {
            panic(NULL, "Thread in invalid state in wait_block_setup() state=%d", expected);
        }
        // We wait until the wait_block_commit() or wait_block_cancel() to actually do the early unblock.
    }

    // Return without enabling interrupts, they will be enabled in wait_block_commit() or wait_block_cancel().
    return 0;
}

void wait_block_cancel(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    thread_t* thread = smp_self_unsafe()->sched.runThread;
    assert(thread != NULL);

    thread_state_t state = atomic_exchange(&thread->state, THREAD_UNBLOCKING);

    // State might already be unblocking if the thread unblocked prematurely.
    assert(state == THREAD_PRE_BLOCK || state == THREAD_UNBLOCKING);

    wait_remove_wait_entries(thread, EOK);

    thread_state_t newState = atomic_exchange(&thread->state, THREAD_RUNNING);
    assert(newState == THREAD_UNBLOCKING); // Make sure state did not change.

    smp_put(); // Release cpu from wait_block_setup().
    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
}

uint64_t wait_block_commit(void)
{
    thread_t* thread = smp_self_unsafe()->sched.runThread;

    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    thread_state_t state = atomic_load(&thread->state);
    switch (state)
    {
    case THREAD_UNBLOCKING:
        wait_remove_wait_entries(thread, EOK);
        atomic_store(&thread->state, THREAD_RUNNING);
        smp_put(); // Release cpu from wait_block_setup().
        break;
    case THREAD_PRE_BLOCK:
        smp_put(); // Release cpu from wait_block_setup().
        timer_notify_self();
        break;
    default:
        panic(NULL, "Invalid thread state %d in wait_block_commit()", state);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    if (thread->wait.err != EOK)
    {
        errno = thread->wait.err;
        return ERR;
    }
    return 0;
}
