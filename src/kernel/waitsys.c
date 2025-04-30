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

static list_t threads;
static lock_t threadsLock;

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
}

void waitsys_cpu_ctx_init(waitsys_cpu_ctx_t* waitsys)
{
    list_init(&waitsys->parkedThreads);
}

void waitsys_init(void)
{
    list_init(&threads);
    lock_init(&threadsLock);
}

// Sets up a threads waitsys ctx but does not yet block
static uint64_t waitsys_thread_setup(thread_t* thread, wait_queue_t** waitQueues, uint64_t amount, nsec_t timeout, lock_t* lock)
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
    thread->waitsys.lock = lock;

    return 0;
}

static void waitsys_thread_unblock(thread_t* thread, block_result_t result, wait_queue_t* acquiredQueue, bool lockAcquired)
{
    thread->waitsys.result = result;
    if (lockAcquired)
    {
        list_remove(&thread->entry);
    }
    else
    {
        LOCK_DEFER(&threadsLock);
        list_remove(&thread->entry);
    }

    for (uint64_t i = 0; i < thread->waitsys.entryAmount; i++)
    {
        if (thread->waitsys.waitEntries[i] == NULL)
        {
            break;
        }
        wait_queue_entry_t* entry = thread->waitsys.waitEntries[i];
        thread->waitsys.waitEntries[i] = NULL;

        if (acquiredQueue != entry->waitQueue)
        {
            LOCK_DEFER(&entry->waitQueue->lock);
            list_remove(&entry->entry);
        }
        else
        {
            list_remove(&entry->entry);
        }
        free(entry);
    }

    sched_push(thread);
}

void waitsys_timer_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();

    while (1)
    {
        thread_t* thread = LIST_CONTAINER_SAFE(list_pop(&self->waitsys.parkedThreads), thread_t, entry);
        if (thread == NULL)
        {
            break;
        }

        LOCK_DEFER(&threadsLock);
        list_push(&threads, &thread->entry);

        for (uint64_t i = 0; i < thread->waitsys.entryAmount; i++)
        {
            wait_queue_entry_t* waitEntry = thread->waitsys.waitEntries[i];
            wait_queue_t* waitQueue = waitEntry->waitQueue;
            LOCK_DEFER(&waitQueue->lock);

            if (waitEntry->cancelBlock)
            {
                waitsys_thread_unblock(thread, BLOCK_NORM, waitQueue, true);
                break;
            }
            waitEntry->blocking = true;
        }
    }

    if (self->id != 0)
    {
        return;
    }

    LOCK_DEFER(&threadsLock);

    // TODO: This is O(n)... fix that
    thread_t* thread;
    thread_t* temp;
    LIST_FOR_EACH_SAFE(thread, temp, &threads, entry)
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

        waitsys_thread_unblock(thread, result, NULL, true);
    }
}

void waitsys_block_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_ctx_t* sched = &self->sched;
    waitsys_cpu_ctx_t* waitsys = &self->waitsys;

    thread_t* thread = sched->runThread;
    sched->runThread = NULL;

    for (uint64_t i = 0; i < thread->waitsys.entryAmount; i++)
    {
        wait_queue_entry_t* waitEntry = thread->waitsys.waitEntries[i];
        wait_queue_t* waitQueue = waitEntry->waitQueue;
        LOCK_DEFER(&waitQueue->lock);

        list_push(&waitQueue->entries, &waitEntry->entry);
    }

    thread_save(thread, trapFrame);
    list_push(&waitsys->parkedThreads, &thread->entry);

    if (thread->waitsys.lock != NULL)
    {
        cpu_t* self = smp_self_unsafe();
        self->cli.depth = UINT64_MAX; // Prevent interrupts from being enabled when releasing lock.
        lock_release(thread->waitsys.lock);
        // Interrupts are now still disabled, set interrupts to be enabled when returning from trap.
        self->cli.depth = 0;
        thread->trapFrame.rflags |= RFLAGS_INTERRUPT_ENABLE;
    }

    sched_schedule_trap(trapFrame);
}

block_result_t waitsys_block(wait_queue_t* waitQueue, nsec_t timeout)
{
    if (timeout == 0)
    {
        return BLOCK_TIMEOUT;
    }

    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (waitsys_thread_setup(thread, &waitQueue, 1, timeout, NULL) == ERR)
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
    ASSERT_PANIC(smp_self_unsafe()->cli.depth == 1); // Only one lock is allowed to be acquired when calling this function.

    thread_t* thread = smp_self_unsafe()->sched.runThread;
    if (waitsys_thread_setup(thread, &waitQueue, 1, timeout, lock) == ERR)
    {
        return BLOCK_ERROR;
    }

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
    if (waitsys_thread_setup(thread, waitQueues, amount, timeout, NULL) == ERR)
    {
        smp_put();
        return BLOCK_ERROR;
    }
    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAITSYS_BLOCK));

    return thread->waitsys.result;
}

void waitsys_unblock(wait_queue_t* waitQueue, uint64_t amount)
{
    LOCK_DEFER(&waitQueue->lock);

    wait_queue_entry_t* entry;
    wait_queue_entry_t* temp;
    LIST_FOR_EACH_SAFE(entry, temp, &waitQueue->entries, entry)
    {
        if (amount == 0)
        {
            return;
        }

        thread_t* thread = entry->thread;
        if (entry->blocking)
        {
            waitsys_thread_unblock(thread, BLOCK_NORM, waitQueue, false);
        }
        else // If the thread has not fully blocked yet.
        {
            entry->cancelBlock = true; // Tell waitsys to unblock before blocking is fully perfomed.
        }
        amount--;
    }
}
