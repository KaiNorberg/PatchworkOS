#include "thread.h"

#include "cpu/gdt.h"
#include "cpu/regs.h"
#include "cpu/smp.h"
#include "defs.h"
#include "drivers/systime/systime.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "mem/heap.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "sync/futex.h"
#include "sync/lock.h"
#include "sys/io.h"

#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

thread_t* thread_new(process_t* process, void* entry)
{
    LOCK_SCOPE(&process->threads.lock);

    if (process->threads.isDying)
    {
        return NULL;
    }

    thread_t* thread = heap_alloc(sizeof(thread_t), HEAP_NONE);
    if (thread == NULL)
    {
        return NULL;
    }
    list_entry_init(&thread->entry);
    thread->process = process;
    list_entry_init(&thread->processEntry);
    thread->id = thread->process->threads.newTid++;
    sched_thread_ctx_init(&thread->sched);
    atomic_init(&thread->state, THREAD_PARKED);
    thread->error = 0;
    wait_thread_ctx_init(&thread->wait);
    if (simd_ctx_init(&thread->simd) == ERR)
    {
        heap_free(thread);
        return NULL;
    }
    note_queue_init(&thread->notes);
    syscall_ctx_init(&thread->syscall, THREAD_KERNEL_STACK_TOP(thread));
    memset(&thread->trapFrame, 0, sizeof(trap_frame_t));
    thread->canary = THREAD_CANARY;
    thread->trapFrame.rip = (uint64_t)entry;
    thread->trapFrame.rsp = THREAD_KERNEL_STACK_TOP(thread);
    thread->trapFrame.cs = GDT_KERNEL_CODE;
    thread->trapFrame.ss = GDT_KERNEL_DATA;
    thread->trapFrame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
    memset(&thread->kernelStack, 0, CONFIG_KERNEL_STACK);

    list_push(&process->threads.list, &thread->processEntry);
    LOG_DEBUG("created tid=%d pid=%d entry=%p\n", thread->id, process->id, entry);
    return thread;
}

void thread_free(thread_t* thread)
{
    LOG_DEBUG("freeing tid=%d pid=%d\n", thread->id, thread->process->id);
    process_t* process = thread->process;

    lock_acquire(&process->threads.lock);
    list_remove(&thread->processEntry);

    // If the entire process is dying then there is no point in unmaping the stack as all memory will be unmapped
    // anyway.
    if (!process->threads.isDying)
    {
        space_unmap(&process->space, (void*)LOADER_USER_STACK_BOTTOM(thread->id),
            CONFIG_MAX_USER_STACK_PAGES * PAGE_SIZE); // Ignore failure
    }

    if (list_is_empty(&process->threads.list))
    {
        lock_release(&process->threads.lock);
        process_free(process);
    }
    else
    {
        lock_release(&process->threads.lock);
    }

    simd_ctx_deinit(&thread->simd);
    heap_free(thread);
}

void thread_save(thread_t* thread, const trap_frame_t* trapFrame)
{
    simd_ctx_save(&thread->simd);
    thread->trapFrame = *trapFrame;
}

void thread_load(thread_t* thread, trap_frame_t* trapFrame)
{
    cpu_t* self = smp_self_unsafe();

    *trapFrame = thread->trapFrame;

    space_load(&thread->process->space);
    tss_stack_load(&self->tss, (void*)THREAD_KERNEL_STACK_TOP(thread));
    simd_ctx_load(&thread->simd);
    syscall_ctx_load(&thread->syscall);
}

bool thread_is_note_pending(thread_t* thread)
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
        wait_unblock_thread(thread, WAIT_NOTE);
    }

    return 0;
}

SYSCALL_DEFINE(SYS_ERRNO, errno_t)
{
    return errno;
}

SYSCALL_DEFINE(SYS_GETTID, tid_t)
{
    return sched_thread()->id;
}
