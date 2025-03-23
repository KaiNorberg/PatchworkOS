#include "waitsys.h"

#include "sched.h"
#include "time.h"
#include "smp.h"
#include "vectors.h"
#include "regs.h"

#include <stdlib.h>
#include <stdio.h>

static list_t threads;
static lock_t threadsLock;

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

    lock_acquire(&blocker->lock);
    list_push(&blocker->entires, entry);
    lock_release(&blocker->lock);
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
        while (!atomic_load(&thread->block.blocking))
        {
            asm volatile("pause");
        }

        thread->block.result = BLOCK_NORM;

        lock_acquire(&threadsLock);
        list_remove(thread);
        lock_release(&threadsLock);
    
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

        atomic_store(&thread->block.blocking, false);
        sched_push(thread);
    }
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
            smp_put();
            for (uint64_t j = 0; j < i; j++)
            {
                free(thread->block.blockEntires[i]);
            }
            thread->error = ENOMEM;
            return BLOCK_ERROR;
        }
        list_entry_init(&entry->entry);
        entry->blocker = blockers[i];
        entry->thread = thread;
    
        thread->block.blockEntires[i] = entry;
    
        lock_acquire(&blockers[i]->lock);
        list_push(&blockers[i]->entires, entry);
        lock_release(&blockers[i]->lock);
    }

    thread->block.entryAmount = amount;
    thread->block.result = BLOCK_NORM;
    thread->block.deadline = timeout == NEVER ? NEVER : time_uptime() + timeout;

    smp_put();

    asm volatile("int %0" :: "i" (VECTOR_WAITSYS_BLOCK));

    return thread->block.result;
}

void waitsys_init(void)
{
    list_init(&threads);
    lock_init(&threadsLock);
}

void waitsys_update(void)
{
    LOCK_DEFER(&threadsLock);

    if (list_empty(&threads))
    {
        return;
    }

    thread_t* thread = list_first(&threads);
    while (!atomic_load(&thread->block.blocking))
    {
        asm volatile("pause");
    }

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
        
        atomic_store(&thread->block.blocking, false);
        sched_push(thread);
    }
}

void waitsys_block(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_context_t* context = &self->sched;

    LOCK_DEFER(&threadsLock);

    thread_save(context->runThread, trapFrame);

    thread_t* thread;
    LIST_FOR_EACH(thread, &threads)
    {
        if (thread->block.deadline > context->runThread->block.deadline)
        {
            list_prepend(&thread->entry, context->runThread);
            atomic_store(&context->runThread->block.blocking, true);
            goto next_thread;
        }
    }

    list_push(&threads, context->runThread);
    atomic_store(&context->runThread->block.blocking, true);

next_thread:
    context->runThread = NULL;
    sched_schedule(trapFrame);
}
