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

void blocker_init(blocker_t* blocker)
{
    lock_init(&blocker->lock);
    list_init(&blocker->entires);
}

void blocker_deinit(blocker_t* blocker)
{
    LOCK_DEFER(&blocker->lock);

    if (!list_empty(&blocker->entires))
    {
        log_panic(NULL, "Blocker with pending threads freed");
    }
}

block_result_t blocker_block(blocker_t* blocker, nsec_t timeout)
{
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE, "blocker_block, interupts disabled");

    thread_t* thread = smp_self()->sched.runThread;
    blocker_entry_t* entry = malloc(sizeof(blocker_entry_t));
    if (entry == NULL)
    {
        smp_put();
        thread->error = ENOMEM;
        return BLOCK_ERROR;
    }
    list_entry_init(&entry->entry);
    entry->blocker = blocker;
    entry->thread = thread;

    thread->block.blockEntires[0] = entry;
    thread->block.entryAmount = 1;
    thread->block.result = BLOCK_NORM;
    thread->block.deadline = timeout == NEVER ? NEVER : time_uptime() + timeout;

    smp_put();

    asm volatile("int %0" :: "i" (VECTOR_WAITSYS_BLOCK));

    return thread->block.result;
}

block_result_t blocker_block_many(blocker_t** blockers, uint64_t amount, nsec_t timeout)
{    
    ASSERT_PANIC(rflags_read() & RFLAGS_INTERRUPT_ENABLE, "blocker_block_many, interupts disabled");

    thread_t* thread = smp_self()->sched.runThread;    
    if (amount > CONFIG_MAX_BLOCKERS_PER_THREAD)
    {
        smp_put();
        thread->error = EBLOCKLIMIT;
        return BLOCK_ERROR;
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        blocker_entry_t* entry = malloc(sizeof(blocker_entry_t));
        if (entry == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                free(thread->block.blockEntires[j]);
            }
            smp_put();
            thread->error = ENOMEM;
            return BLOCK_ERROR;
        }
        list_entry_init(&entry->entry);
        entry->blocker = blockers[i];
        entry->thread = thread;
    
        thread->block.blockEntires[i] = entry;
    }

    thread->block.entryAmount = amount;
    thread->block.result = BLOCK_NORM;
    thread->block.deadline = timeout == NEVER ? NEVER : time_uptime() + timeout;

    smp_put();

    asm volatile("int %0" :: "i" (VECTOR_WAITSYS_BLOCK));

    return thread->block.result;
}

void blocker_unblock(blocker_t* blocker)
{
    LOCK_DEFER(&blocker->lock);

    blocker_entry_t* entry;
    blocker_entry_t* temp;
    LIST_FOR_EACH_SAFE(entry, temp, &blocker->entires)
    {
        thread_t* thread = entry->thread;

        thread->block.result = BLOCK_NORM;
        blocked_threads_remove(thread);
    
        for (uint64_t i = 0; i < thread->block.entryAmount; i++)
        {
            if (thread->block.blockEntires[i] == NULL)
            {
                break;
            }
            blocker_entry_t* entry = thread->block.blockEntires[i];
            thread->block.blockEntires[i] = NULL;
        
            list_remove(entry);
            free(entry);
        }

        sched_push(thread);
    }
}

void waitsys_init(void)
{
    list_init(&blockedThreads);
    lock_init(&blockedThreadsLock);
}

void waitsys_update(trap_frame_t* trapFrame)
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
            blocker_entry_t* entry = thread->block.blockEntires[i];
            thread->block.blockEntires[i] = NULL;
    
            LOCK_DEFER(&entry->blocker->lock);
    
            list_remove(entry);
            free(entry);
        }
        
        sched_push(thread);
    }
}

void waitsys_block(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_context_t* sched = &self->sched;

    thread_save(sched->runThread, trapFrame);

    for (uint64_t i = 0; i < sched->runThread->block.entryAmount; i++)
    {
        blocker_t* blocker = sched->runThread->block.blockEntires[i]->blocker;
        LOCK_DEFER(&blocker->lock);

        list_push(&blocker->entires, sched->runThread->block.blockEntires[i]);
    }

    blocked_threads_add(sched->runThread);

    sched->runThread = NULL;
    sched_schedule(trapFrame);
}
