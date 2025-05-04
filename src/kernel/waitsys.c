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
    waitsys->waitEntries[0] = NULL;
    waitsys->entryAmount = 0;
    waitsys->result = BLOCK_NORM;
    waitsys->deadline = 0;
    waitsys->owner = NULL;
}

static void waitsys_thread_ctx_acquire_all(waitsys_thread_ctx_t* waitsys, wait_queue_t* acquiredQueue)
{
    for (uint64_t i = 0; i < waitsys->entryAmount; i++)
    {
        wait_queue_entry_t* waitEntry = waitsys->waitEntries[i];
        if (waitEntry->waitQueue != acquiredQueue)
        {
            lock_acquire(&waitEntry->waitQueue->lock);
        }
    }
}

static void waitsys_thread_ctx_release_all(waitsys_thread_ctx_t* waitsys, wait_queue_t* acquiredQueue)
{
    for (uint64_t i = 0; i < waitsys->entryAmount; i++)
    {
        wait_queue_entry_t* waitEntry = waitsys->waitEntries[i];
        if (waitEntry->waitQueue != acquiredQueue)
        {
            lock_release(&waitEntry->waitQueue->lock);
        }
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
        for (uint64_t i = 0; i < thread->waitsys.entryAmount; i++)
        {
            wait_queue_entry_t* waitEntry = thread->waitsys.waitEntries[i];

            if (waitEntry->cancelBlock)
            {
                shouldUnblock = true;
                break;
            }
            else
            {
                waitEntry->blocking = true;
            }
        }

        if (shouldUnblock)
        {
            thread->waitsys.result = BLOCK_NORM;

            for (uint64_t i = 0; i < thread->waitsys.entryAmount; i++)
            {
                wait_queue_entry_t* entry = thread->waitsys.waitEntries[i];
                thread->waitsys.waitEntries[i] = NULL;

                list_remove(&entry->entry);
                lock_release(&entry->waitQueue->lock);
                free(entry);
            }

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
        if (atomic_load(&thread->process->dead) || thread->dead)
        {
            result = BLOCK_DEAD;
        }
        else if (systime_uptime() >= thread->waitsys.deadline)
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

        for (uint64_t i = 0; i < thread->waitsys.entryAmount; i++)
        {
            wait_queue_entry_t* entry = thread->waitsys.waitEntries[i];
            thread->waitsys.waitEntries[i] = NULL;

            list_remove(&entry->entry);
            lock_release(&entry->waitQueue->lock);
            free(entry);
        }

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

    wait_queue_entry_t* waitEntry;
    wait_queue_entry_t* temp;
    LIST_FOR_EACH_SAFE(waitEntry, temp, &waitQueue->entries, entry)
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

        for (uint64_t i = 0; i < thread->waitsys.entryAmount; i++)
        {
            wait_queue_entry_t* entry = thread->waitsys.waitEntries[i];
            thread->waitsys.waitEntries[i] = NULL;

            list_remove(&entry->entry);
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
static uint64_t waitsys_thread_setup(thread_t* thread, wait_queue_t** waitQueues, uint64_t amount, nsec_t timeout)
{
    if (thread->dead)
    {
        return ERROR(EINVAL);
    }

    if (amount > CONFIG_MAX_BLOCKERS_PER_THREAD)
    {
        return ERROR(EBLOCKLIMIT);
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        wait_queue_entry_t* entry = malloc(sizeof(wait_queue_entry_t));
        if (entry == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                free(thread->waitsys.waitEntries[j]);
            }
            return ERROR(ENOMEM);
        }
        list_entry_init(&entry->entry);
        entry->waitQueue = waitQueues[i];
        entry->thread = thread;
        entry->blocking = false;
        entry->cancelBlock = false;

        thread->waitsys.waitEntries[i] = entry;
    }

    thread->waitsys.entryAmount = amount;
    thread->waitsys.result = BLOCK_NORM;
    thread->waitsys.deadline = timeout == NEVER ? NEVER : systime_uptime() + timeout;
    thread->waitsys.owner = NULL;

    for (uint64_t i = 0; i < amount; i++)
    {
        list_push(&waitQueues[i]->entries, &thread->waitsys.waitEntries[i]->entry);
    }

    return 0;
}

block_result_t waitsys_block(wait_queue_t* waitQueue, nsec_t timeout)
{
    if (timeout == 0)
    {
        return BLOCK_TIMEOUT;
    }

    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (waitsys_thread_setup(thread, &waitQueue, 1, timeout) == ERR)
    {
        smp_put();
        return BLOCK_ERROR;
    }
    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAITSYS_BLOCK));

    return thread->waitsys.result;
}

block_result_t waitsys_block_lock(wait_queue_t* waitQueue, nsec_t timeout, lock_t* lock)
{
    if (timeout == 0)
    {
        return BLOCK_TIMEOUT;
    }

    ASSERT_PANIC(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));
    ASSERT_PANIC(
        smp_self_unsafe()->cli.depth == 1); // Only one lock is allowed to be acquired when calling this function.

    thread_t* thread = smp_self_unsafe()->sched.runThread;
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

block_result_t waitsys_block_many(wait_queue_t** waitQueues, uint64_t amount, nsec_t timeout)
{
    if (timeout == 0)
    {
        return BLOCK_TIMEOUT;
    }

    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (waitsys_thread_setup(thread, waitQueues, amount, timeout) == ERR)
    {
        smp_put();
        return BLOCK_ERROR;
    }
    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAITSYS_BLOCK));

    return thread->waitsys.result;
}
