#include "thread.h"

#include "cpu/gdt.h"
#include "cpu/regs.h"
#include "cpu/smp.h"
#include "defs.h"
#include "drivers/systime/systime.h"
#include "fs/path.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "sync/futex.h"
#include "sync/lock.h"
#include "sys/io.h"
#include "utils/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

thread_t* thread_new(process_t* process, void* entry, priority_t priority)
{
    LOCK_DEFER(&process->threads.lock);

    if (process->threads.dying)
    {
        return NULL;
    }

    thread_t* thread = malloc(sizeof(thread_t));
    if (thread == NULL)
    {
        return NULL;
    }
    list_entry_init(&thread->entry);
    thread->process = process;
    list_entry_init(&thread->processEntry);
    thread->id = thread->process->threads.newTid++;
    thread->timeStart = 0;
    thread->timeEnd = 0;
    thread->priority = MIN(priority, PRIORITY_MAX);
    atomic_init(&thread->state, THREAD_PARKED);
    thread->error = 0;
    wait_thread_ctx_init(&thread->wait);
    if (simd_ctx_init(&thread->simd) == ERR)
    {
        free(thread);
        return NULL;
    }
    note_queue_init(&thread->notes);
    memset(&thread->trapFrame, 0, sizeof(trap_frame_t));
    thread->trapFrame.rip = (uint64_t)entry;
    thread->trapFrame.rsp = ((uint64_t)thread->kernelStack) + CONFIG_KERNEL_STACK;
    thread->trapFrame.cs = GDT_KERNEL_CODE;
    thread->trapFrame.ss = GDT_KERNEL_DATA;
    thread->trapFrame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
    memset(&thread->kernelStack, 0, CONFIG_KERNEL_STACK);

    list_push(&process->threads.list, &thread->processEntry);
    return thread;
}

void thread_free(thread_t* thread)
{
    lock_acquire(&thread->process->threads.lock);
    list_remove(&thread->processEntry);

    if (list_empty(&thread->process->threads.list))
    {
        lock_release(&thread->process->threads.lock);
        process_free(thread->process);
    }
    else
    {
        lock_release(&thread->process->threads.lock);
    }

    simd_ctx_deinit(&thread->simd);
    free(thread);
}

void thread_save(thread_t* thread, const trap_frame_t* trapFrame)
{
    simd_ctx_save(&thread->simd);
    thread->trapFrame = *trapFrame;
}

void thread_load(thread_t* thread, trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();

    if (thread == NULL)
    {
        memset(trapFrame, 0, sizeof(trap_frame_t));
        trapFrame->rip = (uint64_t)sched_idle_loop;
        trapFrame->cs = GDT_KERNEL_CODE;
        trapFrame->ss = GDT_KERNEL_DATA;
        trapFrame->rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
        trapFrame->rsp = (uint64_t)self->idleStack + CPU_IDLE_STACK_SIZE;

        space_load(NULL);
        tss_stack_load(&self->tss, NULL);
    }
    else
    {
        thread->timeStart = systime_uptime();
        thread->timeEnd = thread->timeStart + CONFIG_TIME_SLICE;

        *trapFrame = thread->trapFrame;

        space_load(&thread->process->space);
        tss_stack_load(&self->tss, (void*)((uint64_t)thread->kernelStack + CONFIG_KERNEL_STACK));
        simd_ctx_load(&thread->simd);
    }
}

bool thread_note_pending(thread_t* thread)
{
    return note_queue_length(&thread->notes) != 0;
}

uint64_t thread_send_note(thread_t* thread, const void* message, uint64_t length)
{
    note_flags_t flags = 0;
    if (strncmp(message, "kill", length) == 0)
    {
        flags |= NOTE_CRITICAL;
    }

    if (note_queue_push(&thread->notes, message, length, flags) == ERR)
    {
        return ERR;
    }

    thread_state_t expected = THREAD_BLOCKED;
    if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
    {
        wait_unblock_thread(thread, WAIT_NOTE, NULL, true);
    }

    return 0;
}