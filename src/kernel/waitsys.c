#include "waitsys.h"

#include "log.h"
#include "regs.h"
#include "sched.h"
#include "smp.h"
#include "systime.h"
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
        log_panic(NULL, "WaitQueue with pending threads freed");
    }
}

void waitsys_context_init(waitsys_context_t* waitsys)
{
    list_init(&waitsys->threads);
    lock_init(&waitsys->lock);
}

static void waitsys_context_add(waitsys_context_t* waitsys, thread_t* thread)
{
    LOCK_DEFER(&waitsys->lock);

    if (thread->block.deadline == NEVER)
    {
        list_push(&waitsys->threads, &thread->entry);
        return;
    }

    thread_t* other;
    LIST_FOR_EACH(other, &waitsys->threads, entry)
    {
        if (other->block.deadline > thread->block.deadline)
        {
            list_prepend(&other->entry, &thread->entry);
            return;
        }
    }
    list_push(&waitsys->threads, &thread->entry);
}

static void waitsys_context_remove(waitsys_context_t* waitsys, thread_t* thread)
{
    LOCK_DEFER(&waitsys->lock);
    list_remove(&thread->entry);
}

void waitsys_update_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    waitsys_context_t* waitsys = &self->waitsys;

    LOCK_DEFER(&waitsys->lock);

    if (list_empty(&waitsys->threads))
    {
        return;
    }

    // TODO: This is O(n)... fix that
    thread_t* thread;
    thread_t* temp;
    LIST_FOR_EACH_SAFE(thread, temp, &waitsys->threads, entry)
    {
        if (systime_uptime() < thread->block.deadline && !(thread->process->killed || thread->killed))
        {
            continue;
        }

        if (thread->process->killed || thread->killed)
        {
            thread->block.result = BLOCK_KILLED;
        }
        else
        {
            thread->block.result = BLOCK_TIMEOUT;
        }

        list_remove(&thread->entry);

        for (uint64_t i = 0; i < thread->block.entryAmount; i++)
        {
            wait_queue_entry_t* entry = thread->block.waitEntries[i];
            thread->block.waitEntries[i] = NULL;

            LOCK_DEFER(&entry->waitQueue->lock);

            list_remove(&entry->entry);
            free(entry);
        }

        sched_push(thread);
    }
}

void waitsys_block_trap(trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();
    sched_context_t* sched = &self->sched;
    waitsys_context_t* waitsys = &self->waitsys;

    thread_save(sched->runThread, trapFrame);

    for (uint64_t i = 0; i < sched->runThread->block.entryAmount; i++)
    {
        wait_queue_t* waitQueue = sched->runThread->block.waitEntries[i]->waitQueue;
        LOCK_DEFER(&waitQueue->lock);

        list_push(&waitQueue->entries, &sched->runThread->block.waitEntries[i]->entry);
    }

    sched->runThread->block.waitsys = waitsys;
    waitsys_context_add(waitsys, sched->runThread);

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
    thread->block.deadline = timeout == NEVER ? NEVER : systime_uptime() + timeout;

    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAITSYS_BLOCK));

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
    thread->block.deadline = timeout == NEVER ? NEVER : systime_uptime() + timeout;

    smp_put();

    asm volatile("int %0" ::"i"(VECTOR_WAITSYS_BLOCK));

    return thread->block.result;
}

void waitsys_unblock(wait_queue_t* waitQueue)
{
    LOCK_DEFER(&waitQueue->lock);

    wait_queue_entry_t* entry;
    wait_queue_entry_t* temp;
    LIST_FOR_EACH_SAFE(entry, temp, &waitQueue->entries, entry)
    {
        thread_t* thread = entry->thread;

        thread->block.result = BLOCK_NORM;
        waitsys_context_remove(thread->block.waitsys, thread);

        for (uint64_t i = 0; i < thread->block.entryAmount; i++)
        {
            if (thread->block.waitEntries[i] == NULL)
            {
                break;
            }
            wait_queue_entry_t* entry = thread->block.waitEntries[i];
            thread->block.waitEntries[i] = NULL;

            list_remove(&entry->entry);
            free(entry);
        }

        sched_push(thread);
    }
}