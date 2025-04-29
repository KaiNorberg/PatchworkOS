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
        log_panic(NULL, "WaitQueue with pending threads freed");
    }
}

void waitsys_ctx_init(waitsys_ctx_t* waitsys)
{
    waitsys->waitEntries[0] = NULL;
    waitsys->entryAmount = 0;
    waitsys->result = BLOCK_NORM;
    waitsys->deadline = 0;
}

static void waitsys_add(thread_t* thread)
{
    LOCK_DEFER(&threadsLock);

    if (thread->waitsys.deadline == NEVER)
    {
        list_push(&threads, &thread->entry);
        return;
    }

    thread_t* other;
    LIST_FOR_EACH(other, &threads, entry)
    {
        if (other->waitsys.deadline > thread->waitsys.deadline)
        {
            list_prepend(&other->entry, &thread->entry);
            return;
        }
    }
    list_push(&threads, &thread->entry);
}

void waitsys_init(void)
{
    list_init(&threads);
    lock_init(&threadsLock);
}

static uint64_t waitsys_thread_block(thread_t* thread, wait_queue_t** waitQueues, uint64_t amount, nsec_t timeout, lock_t* lock)
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

    thread_t* thread = sched->runThread;
    sched->runThread = NULL;

    for (uint64_t i = 0; i < thread->waitsys.entryAmount; i++)
    {
        wait_queue_t* waitQueue = thread->waitsys.waitEntries[i]->waitQueue;
        LOCK_DEFER(&waitQueue->lock);
        list_push(&waitQueue->entries, &thread->waitsys.waitEntries[i]->entry);
    }

    waitsys_add(thread);
    thread_save(thread, trapFrame);
    sched_schedule_trap(trapFrame);

    if (thread->waitsys.lock != NULL)
    {
        cpu_t* self = smp_self_unsafe();
        self->cli.depth = UINT64_MAX; // Prevent interrupts from being enabled when releasing lock.
        lock_release(thread->waitsys.lock);
        // Interrupts are now still disabled, set interrupts to be enabled when returning from trap.
        self->cli.depth = 0;
        thread->trapFrame.rflags |= RFLAGS_INTERRUPT_ENABLE;
    }
}

block_result_t waitsys_block(wait_queue_t* waitQueue, nsec_t timeout)
{
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (waitsys_thread_block(thread, &waitQueue, 1, timeout, NULL) == ERR)
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
    ASSERT_PANIC(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    thread_t* thread = smp_self_unsafe()->sched.runThread;
    if (waitsys_thread_block(thread, &waitQueue, 1, timeout, lock) == ERR)
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
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE);

    thread_t* thread = smp_self()->sched.runThread;
    if (waitsys_thread_block(thread, waitQueues, amount, timeout, NULL) == ERR)
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
        waitsys_thread_unblock(thread, BLOCK_NORM, waitQueue, false);
        amount--;
    }
}
