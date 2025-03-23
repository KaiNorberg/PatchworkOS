#include "waitsys.h"

#include "sched.h"
#include "time.h"
#include "smp.h"
#include "vectors.h"
#include "regs.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

static list_t blockedThreads;
static lock_t blockedThreadsLock;

static void blocked_threads_add(thread_t* thread)
{
    LOCK_DEFER(&blockedThreadsLock);

    if (thread->block.deadline == NEVER)
    {
        list_push(&blockedThreads, thread);
        return;
    }

    thread_t* other;
    LIST_FOR_EACH(other, &blockedThreads)
    {
        if (other->block.deadline > thread->block.deadline)
        {
            list_prepend(&other->entry, thread);
            return;
        }
    }
    list_push(&blockedThreads, thread);
}

static void blocked_threads_remove(thread_t* thread)
{
    LOCK_DEFER(&blockedThreadsLock);
    list_remove(thread);
}

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

void waitsys_init(void)
{
    list_init(&blockedThreads);
    lock_init(&blockedThreadsLock);
}

void waitsys_update_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();

    LOCK_DEFER(&blockedThreadsLock);

    if (list_empty(&blockedThreads))
    {
        return;
    }

    thread_t* thread = list_first(&blockedThreads);
    if (time_uptime() >= thread->block.deadline)
    {            
        thread->block.result = BLOCK_TIMEOUT;

        list_remove(thread);
    
        for (uint64_t i = 0; i < thread->block.entryAmount; i++)
        {
            wait_queue_entry_t* entry = thread->block.waitEntries[i];
            thread->block.waitEntries[i] = NULL;
    
            LOCK_DEFER(&entry->waitQueue->lock);
    
            list_remove(entry);
            free(entry);
        }
        
        sched_push(thread);
    }
}

void waitsys_block_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_context_t* sched = &self->sched;

    thread_save(sched->runThread, trapFrame);

    for (uint64_t i = 0; i < sched->runThread->block.entryAmount; i++)
    {
        wait_queue_t* waitQueue = sched->runThread->block.waitEntries[i]->waitQueue;
        LOCK_DEFER(&waitQueue->lock);

        list_push(&waitQueue->entries, sched->runThread->block.waitEntries[i]);
    }

    blocked_threads_add(sched->runThread);

    sched->runThread = NULL;
    sched_schedule_trap(trapFrame);
}

block_result_t waitsys_block(wait_queue_t* waitQueue, nsec_t timeout)
{
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE, "waitsys_block, interupts disabled");

    thread_t* thread = smp_self()->sched.runThread;
    wait_queue_entry_t* entry = malloc(sizeof(wait_queue_entry_t));
    if (entry == NULL)
    {
        smp_put();
        thread->error = ENOMEM;
        return BLOCK_ERROR;
    }
    list_entry_init(&entry->entry);
    entry->waitQueue = waitQueue;
    entry->thread = thread;

    thread->block.waitEntries[0] = entry;
    thread->block.entryAmount = 1;
    thread->block.result = BLOCK_NORM;
    thread->block.deadline = timeout == NEVER ? NEVER : time_uptime() + timeout;

    smp_put();

    asm volatile("int %0" :: "i" (VECTOR_WAITSYS_BLOCK));

    return thread->block.result;
}

block_result_t waitsys_block_many(wait_queue_t** waitQueues, uint64_t amount, nsec_t timeout)
{    
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE, "waitsys_block_many, interupts disabled");

    thread_t* thread = smp_self()->sched.runThread;    
    if (amount > CONFIG_MAX_BLOCKERS_PER_THREAD)
    {
        smp_put();
        thread->error = EBLOCKLIMIT;
        return BLOCK_ERROR;
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        wait_queue_entry_t* entry = malloc(sizeof(wait_queue_entry_t));
        if (entry == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                free(thread->block.waitEntries[j]);
            }
            smp_put();
            thread->error = ENOMEM;
            return BLOCK_ERROR;
        }
        list_entry_init(&entry->entry);
        entry->waitQueue = waitQueues[i];
        entry->thread = thread;
    
        thread->block.waitEntries[i] = entry;
    }

    thread->block.entryAmount = amount;
    thread->block.result = BLOCK_NORM;
    thread->block.deadline = timeout == NEVER ? NEVER : time_uptime() + timeout;

    smp_put();

    asm volatile("int %0" :: "i" (VECTOR_WAITSYS_BLOCK));

    return thread->block.result;
}

void waitsys_unblock(wait_queue_t* waitQueue)
{
    LOCK_DEFER(&waitQueue->lock);

    wait_queue_entry_t* entry;
    wait_queue_entry_t* temp;
    LIST_FOR_EACH_SAFE(entry, temp, &waitQueue->entries)
    {
        thread_t* thread = entry->thread;

        thread->block.result = BLOCK_NORM;
        blocked_threads_remove(thread);
    
        for (uint64_t i = 0; i < thread->block.entryAmount; i++)
        {
            if (thread->block.waitEntries[i] == NULL)
            {
                break;
            }
            wait_queue_entry_t* entry = thread->block.waitEntries[i];
            thread->block.waitEntries[i] = NULL;
        
            list_remove(entry);
            free(entry);
        }

        sched_push(thread);
    }
}