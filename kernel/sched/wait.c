#include <kernel/cpu/irq.h>
#include <kernel/mem/cache.h>
#include <kernel/sched/wait.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/ipi.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <libstd/list.h>
#include <stdatomic.h>

PERCPU_DEFINE_CTOR(static wait_t, pcpu_wait)
{
    wait_t* wait = SELF_PTR(pcpu_wait);

    list_init(&wait->blockedThreads);
    lock_init(&wait->lock);
}

static void wait_remove_from_queue(thread_t* thread)
{
    wait_client_t* client = &thread->wait;

    // A queue should never be freed while still having clients so this is probably safe?
    wait_queue_t* queue = client->queue;
    if (queue != NULL)
    {
        lock_acquire(&queue->lock);
        if (client->queue == queue)
        {
            list_remove(&client->queueEntry);
            client->queue = NULL;
        }
        lock_release(&queue->lock);
    }
}

void wait_queue_init(wait_queue_t* queue)
{
    lock_init(&queue->lock);
    list_init(&queue->entries);
}

void wait_queue_deinit(wait_queue_t* queue)
{
    LOCK_SCOPE(&queue->lock);

    if (!list_is_empty(&queue->entries))
    {
        panic(NULL, "Wait queue with pending threads freed");
    }
}

void wait_client_init(wait_client_t* client)
{
    list_entry_init(&client->timeoutEntry);
    list_entry_init(&client->queueEntry);
    client->queue = NULL;
    client->status = OK;
    client->deadline = 0;
    client->owner = NULL;
}

void wait_check_timeouts(interrupt_frame_t* frame)
{
    UNUSED(frame);

    wait_t* wait = SELF_PTR(pcpu_wait);
    LOCK_SCOPE(&wait->lock);

    while (1)
    {
        thread_t* thread = CONTAINER_OF_SAFE(list_first(&wait->blockedThreads), thread_t, wait.timeoutEntry);
        if (thread == NULL)
        {
            return;
        }

        clock_t uptime = clock_uptime();
        if (thread->wait.deadline > uptime)
        {
            timer_set(uptime, thread->wait.deadline);
            return;
        }

        list_remove(&thread->wait.timeoutEntry);

        thread_state_t expected = THREAD_BLOCKED;
        if (!atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING)) // Already unblocking.
        {
            continue;
        }

        thread->wait.status = ERR(SCHED, TIMEOUT);
        wait_remove_from_queue(thread);
        sched_submit(thread);
    }
}

status_t wait_block_prepare(wait_queue_t* queue, clock_t timeout)
{
    if (queue == NULL)
    {
        return ERR(SCHED, INVAL);
    }

    cli_push();

    thread_t* thread = thread_current_unsafe();
    assert(thread != NULL);

    lock_acquire(&queue->lock);

    thread->wait.status = OK;
    thread->wait.deadline = CLOCKS_DEADLINE(timeout, clock_uptime());
    thread->wait.owner = NULL;
    thread->wait.queue = queue;

    list_push_back(&queue->entries, &thread->wait.queueEntry);

    lock_release(&queue->lock);

    thread_state_t expected = THREAD_ACTIVE;
    if (!atomic_compare_exchange_strong(&thread->state, &expected, THREAD_PRE_BLOCK))
    {
        if (expected != THREAD_UNBLOCKING)
        {
            panic(NULL, "Thread in invalid state in wait_block_prepare() state=%d", expected);
        }
        // We wait until the wait_block_commit() or wait_block_cancel() to actually do the early unblock.
    }

    // Return without enabling interrupts, they will be enabled in wait_block_commit() or wait_block_cancel().
    return OK;
}

void wait_block_cancel(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    thread_t* thread = thread_current_unsafe();
    assert(thread != NULL);

    thread_state_t state = atomic_exchange(&thread->state, THREAD_UNBLOCKING);

    // State might already be unblocking if the thread unblocked prematurely.
    assert(state == THREAD_PRE_BLOCK || state == THREAD_UNBLOCKING);

    wait_remove_from_queue(thread);

    thread_state_t newState = atomic_exchange(&thread->state, THREAD_ACTIVE);
    assert(newState == THREAD_UNBLOCKING); // Make sure state did not change.

    cli_pop(); // Release cpu from wait_block_prepare().
    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);
}

status_t wait_block_commit(void)
{
    thread_t* thread = thread_current_unsafe();
    assert(thread != NULL);

    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    thread_state_t state = atomic_load(&thread->state);
    switch (state)
    {
    case THREAD_UNBLOCKING:
        wait_remove_from_queue(thread);
        atomic_store(&thread->state, THREAD_ACTIVE);
        cli_pop(); // Release cpu from wait_block_prepare().
        break;
    case THREAD_PRE_BLOCK:
        cli_pop(); // Release cpu from wait_block_prepare().
        ipi_invoke();
        break;
    default:
        panic(NULL, "Invalid thread state %d in wait_block_commit()", state);
    }

    assert(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    return thread->wait.status;
}

bool wait_block_finalize(interrupt_frame_t* frame, thread_t* thread, clock_t uptime)
{
    UNUSED(frame);

    wait_t* wait = SELF_PTR(pcpu_wait);

    thread->wait.owner = wait;
    LOCK_SCOPE(&wait->lock);

    thread_t* lastThread = (CONTAINER_OF(list_last(&wait->blockedThreads), thread_t, wait.timeoutEntry));

    // Sort blocked threads by deadline
    if (thread->wait.deadline == CLOCKS_NEVER || list_is_empty(&wait->blockedThreads) ||
        lastThread->wait.deadline <= thread->wait.deadline)
    {
        list_push_back(&wait->blockedThreads, &thread->wait.timeoutEntry);
    }
    else
    {
        thread_t* other;
        LIST_FOR_EACH(other, &wait->blockedThreads, wait.timeoutEntry)
        {
            if (other->wait.deadline > thread->wait.deadline)
            {
                list_prepend(&other->wait.timeoutEntry, &thread->wait.timeoutEntry);
                break;
            }
        }
    }

    thread_state_t expected = THREAD_PRE_BLOCK;
    if (!atomic_compare_exchange_strong(&thread->state, &expected, THREAD_BLOCKED)) // Prematurely unblocked
    {
        list_remove(&thread->wait.timeoutEntry);
        wait_remove_from_queue(thread);
        atomic_store(&thread->state, THREAD_ACTIVE);
        return false;
    }

    if (thread_is_note_pending(thread))
    {
        thread_state_t expected = THREAD_BLOCKED;
        if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
        {
            list_remove(&thread->wait.timeoutEntry);
            thread->wait.status = ERR(SCHED, CANCELLED);
            wait_remove_from_queue(thread);
            atomic_store(&thread->state, THREAD_ACTIVE);
            return false;
        }
    }

    timer_set(uptime, thread->wait.deadline);
    return true;
}

void wait_unblock_thread(thread_t* thread, status_t status)
{
    assert(atomic_load(&thread->state) == THREAD_UNBLOCKING);

    lock_acquire(&thread->wait.owner->lock);
    list_remove(&thread->wait.timeoutEntry);
    thread->wait.status = status;
    wait_remove_from_queue(thread);
    lock_release(&thread->wait.owner->lock);

    sched_submit(thread);
}

uint64_t wait_unblock(wait_queue_t* queue, uint64_t amount, status_t status)
{
    uint64_t amountUnblocked = 0;

    const uint64_t threadsPerBatch = 64;
    while (1)
    {
        // For consistent lock ordering we first remove from the wait queue, release the wait queues lock and then
        // acquire the threads cpu lock. Such that every time we acquire the locks its, cpu lock -> wait queue lock.

        thread_t* threads[threadsPerBatch];
        uint64_t toUnblock = amount < threadsPerBatch ? amount : threadsPerBatch;

        lock_acquire(&queue->lock);

        wait_client_t* temp;
        wait_client_t* client;
        uint64_t collected = 0;
        LIST_FOR_EACH_SAFE(client, temp, &queue->entries, queueEntry)
        {
            if (collected == toUnblock)
            {
                break;
            }

            thread_t* thread = CONTAINER_OF(client, thread_t, wait);
            thread_state_t oldState = atomic_exchange(&thread->state, THREAD_UNBLOCKING);

            if (oldState == THREAD_BLOCKED || oldState == THREAD_PRE_BLOCK)
            {
                list_remove(&client->queueEntry);
                client->queue = NULL;
                client->status = status;

                // Only threads that are fully blocked need to be removed from the timeout list and submitted.
                if (oldState == THREAD_BLOCKED)
                {
                    threads[collected] = thread;
                    collected++;
                }
                amountUnblocked++;
            }
        }

        lock_release(&queue->lock);

        if (collected == 0)
        {
            break;
        }

        for (uint64_t i = 0; i < collected; i++)
        {
            lock_acquire(&threads[i]->wait.owner->lock);
            list_remove(&threads[i]->wait.timeoutEntry);
            lock_release(&threads[i]->wait.owner->lock);

            sched_submit(threads[i]);
        }
    }

    return amountUnblocked;
}