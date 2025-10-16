#include "thread.h"

#include "cpu/gdt.h"
#include "cpu/smp.h"
#include "init/init.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sched/sched.h"
#include "sched/timer.h"
#include "sched/wait.h"
#include "sync/lock.h"

#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

static thread_t bootThread;
static bool bootThreadInitalized = false;

uint64_t thread_get_kernel_stack_top(thread_t* thread)
{
    LOG_DEBUG("thread_get_kernel_stack_top: tid=%d top=%p\n", thread->id, thread->kernelStack.top);
    return thread->kernelStack.top;
}

static uintptr_t thread_id_to_offset(tid_t tid, uint64_t maxPages)
{
    return tid * ((maxPages + STACK_POINTER_GUARD_PAGES) * PAGE_SIZE);
}

static uint64_t thread_init(thread_t* thread, process_t* process)
{
    list_entry_init(&thread->entry);
    thread->process = process;
    list_entry_init(&thread->processEntry);
    thread->id = thread->process->threads.newTid++;
    sched_thread_ctx_init(&thread->sched);
    atomic_init(&thread->state, THREAD_PARKED);
    thread->error = 0;
    if (stack_pointer_init(&thread->kernelStack,
            PML_HIGHER_HALF_END - thread_id_to_offset(thread->id, CONFIG_MAX_KERNEL_STACK_PAGES),
            CONFIG_MAX_KERNEL_STACK_PAGES) == ERR)
    {
        return ERR;
    }
    vmm_alloc(&thread->process->space, (void*)thread->kernelStack.bottom, CONFIG_MAX_KERNEL_STACK_PAGES * PAGE_SIZE,
        PML_WRITE | PML_PRESENT);
    if (stack_pointer_init(&thread->userStack,
            PML_LOWER_HALF_END - thread_id_to_offset(thread->id, CONFIG_MAX_USER_STACK_PAGES),
            CONFIG_MAX_USER_STACK_PAGES) == ERR)
    {
        return ERR;
    }
    wait_thread_ctx_init(&thread->wait);
    if (simd_ctx_init(&thread->simd) == ERR)
    {
        return ERR;
    }
    note_queue_init(&thread->notes);
    syscall_ctx_init(&thread->syscall, &thread->kernelStack);
    memset(&thread->trapFrame, 0, sizeof(trap_frame_t));

    list_push(&process->threads.aliveThreads, &thread->processEntry);
    LOG_DEBUG("created tid=%d pid=%d\n", thread->id, process->id);
    return 0;
}

thread_t* thread_new(process_t* process)
{
    LOCK_SCOPE(&process->threads.lock);

    if (atomic_load(&process->isDying))
    {
        return NULL;
    }

    thread_t* thread = heap_alloc(sizeof(thread_t), HEAP_NONE);
    if (thread == NULL)
    {
        return NULL;
    }
    if (thread_init(thread, process) == ERR)
    {
        heap_free(thread);
        return NULL;
    }
    return thread;
}

void thread_free(thread_t* thread)
{
    process_t* process = thread->process;

    assert(atomic_load(&thread->state) == THREAD_ZOMBIE);

    lock_acquire(&process->threads.lock);
    list_remove(&process->threads.zombieThreads, &thread->processEntry);

    if (list_is_empty(&process->threads.aliveThreads) && list_is_empty(&process->threads.zombieThreads))
    {
        lock_release(&process->threads.lock);
        process_free(process);
    }
    else
    {
        lock_release(&process->threads.lock);
    }

    simd_ctx_deinit(&thread->simd);

    stack_pointer_deinit(&thread->kernelStack, thread);
    heap_free(thread);
}

void thread_kill(thread_t* thread)
{
    lock_acquire(&thread->process->threads.lock);

    if (atomic_exchange(&thread->state, THREAD_ZOMBIE) != THREAD_RUNNING)
    {
        panic(NULL, "Invalid state while killing thread");
    }

    list_remove(&thread->process->threads.aliveThreads, &thread->processEntry);
    list_push(&thread->process->threads.zombieThreads, &thread->processEntry);

    if (list_is_empty(&thread->process->threads.aliveThreads))
    {
        // We cant create more alive threads if there are no alive threads, so there is no race condition.
        lock_release(&thread->process->threads.lock);

        // We lose the ability to signal exit status of all threads are killed separately, this appears to be posix
        // behaviour.
        process_kill(thread->process, EXIT_SUCCESS);
    }
    else
    {
        lock_release(&thread->process->threads.lock);
    }
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
    tss_kernel_stack_load(&self->tss, &thread->kernelStack);
    simd_ctx_load(&thread->simd);
    syscall_ctx_load(&thread->syscall);
}

void thread_get_trap_frame(thread_t* thread, trap_frame_t* trapFrame)
{
    *trapFrame = thread->trapFrame;
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

thread_t* thread_get_boot(void)
{
    if (!bootThreadInitalized)
    {
        if (thread_init(&bootThread, process_get_kernel()) == ERR)
        {
            panic(NULL, "Failed to initialize boot thread");
        }

        bootThread.trapFrame.rip = (uintptr_t)kmain;
        bootThread.trapFrame.rsp = bootThread.kernelStack.top;
        bootThread.trapFrame.cs = GDT_CS_RING0;
        bootThread.trapFrame.ss = GDT_SS_RING0;
        bootThread.trapFrame.rflags = RFLAGS_ALWAYS_SET;

        LOG_INFO("boot thread initialized with pid=%d tid=%d\n", bootThread.process->id, bootThread.id);
        bootThreadInitalized = true;
    }
    return &bootThread;
}

uint64_t thread_handle_page_fault(const trap_frame_t* trapFrame)
{
    thread_t* thread = sched_thread();
    if (thread == NULL)
    {
        return ERR;
    }
    uintptr_t faultAddr = (uintptr_t)cr2_read();

    if (trapFrame->errorCode & PAGE_FAULT_PRESENT)
    {
        errno = EFAULT;
        return ERR;
    }

    if (TRAP_FRAME_IN_USER_SPACE(trapFrame))
    {
        if (stack_pointer_handle_page_fault(&thread->userStack, thread, faultAddr,
                PML_WRITE | PML_USER | PML_PRESENT) == ERR)
        {
            return ERR;
        }
        return 0;
    }

    if (stack_pointer_handle_page_fault(&thread->userStack, thread, faultAddr, PML_WRITE | PML_USER | PML_PRESENT) ==
        ERR)
    {
        if (errno != ENOENT) // ENOENT means not in user stack, so try kernel stack.
        {
            return ERR;
        }
        errno = 0;

        if (stack_pointer_handle_page_fault(&thread->kernelStack, thread, faultAddr, PML_WRITE | PML_PRESENT) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}
