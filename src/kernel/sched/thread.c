#include <kernel/cpu/syscall.h>
#include <kernel/sched/thread.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/cache.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/start/start.h>
#include <kernel/sync/lock.h>

#include <stdlib.h>
#include <string.h>
#include <sys/arch.h>
#include <sys/list.h>
#include <sys/math.h>

static void thread_ctor(void* ptr)
{
    thread_t* thread = (thread_t*)ptr;

    thread->process = NULL;
    thread->id = 0;
    list_entry_init(&thread->processEntry);
    atomic_init(&thread->state, THREAD_PARKED);
    thread->error = 0;
    thread->kernelStack = (stack_pointer_t){0};
    thread->userStack = (stack_pointer_t){0};
    thread->wait = (wait_client_t){0};
    thread->simd = (simd_ctx_t){0};
    thread->notes = (note_queue_t){0};
    thread->syscall = (syscall_ctx_t){0};
    thread->perf = (perf_thread_ctx_t){0};
    thread->fsBase = 0;
    memset_s(&thread->frame, sizeof(interrupt_frame_t), 0, sizeof(interrupt_frame_t));
}

static cache_t cache = CACHE_CREATE(cache, "thread", sizeof(thread_t), CACHE_LINE, NULL, NULL);

static uintptr_t thread_id_to_offset(thrd_t tid, uint64_t maxPages)
{
    return tid * ((maxPages + STACK_POINTER_GUARD_PAGES) * PAGE_SIZE);
}

status_t thread_new(thread_t** out, process_t* process)
{
    if (out == NULL || process == NULL)
    {
        return ERR(SCHED, INVAL);
    }

    thread_t* thread = cache_alloc(&cache);
    if (thread == NULL)
    {
        return ERR(SCHED, NOMEM);
    }

    if (atomic_load(&process->flags) & PROCESS_DYING)
    {
        cache_free(thread);
        return ERR(SCHED, DYING);
    }

    thread->process = process;
    thread->id = atomic_fetch_add_explicit(&process->threads.newTid, 1, memory_order_relaxed);
    sched_client_init(&thread->sched);
    atomic_store(&thread->state, THREAD_PARKED);
    thread->error = 0;
    stack_pointer_init(&thread->kernelStack,
        VMM_KERNEL_STACKS_MAX - thread_id_to_offset(thread->id, CONFIG_MAX_KERNEL_STACK_PAGES),
        CONFIG_MAX_KERNEL_STACK_PAGES);
    stack_pointer_init(&thread->userStack,
        VMM_USER_SPACE_MAX - thread_id_to_offset(thread->id, CONFIG_MAX_USER_STACK_PAGES), CONFIG_MAX_USER_STACK_PAGES);
    wait_client_init(&thread->wait);
    status_t status = simd_ctx_init(&thread->simd);
    if (IS_ERR(status))
    {
        cache_free(thread);
        return status;
    }
    note_queue_init(&thread->notes);
    syscall_ctx_init(&thread->syscall, &thread->kernelStack);
    perf_thread_ctx_init(&thread->perf);

    thread->fsBase = 0;

    REF(process);
    lock_acquire(&process->threads.lock);
    process->threads.count++;
    list_push_back_rcu(&process->threads.list, &thread->processEntry);
    lock_release(&process->threads.lock);

    *out = thread;
    return OK;
}

static void thread_rcu_free(void* arg)
{
    thread_t* thread = (thread_t*)arg;

    UNREF(thread->process);
    thread->process = NULL;

    simd_ctx_deinit(&thread->simd);

    cache_free(thread);
}

void thread_free(thread_t* thread)
{
    lock_acquire(&thread->process->threads.lock);
    thread->process->threads.count--;
    list_remove_rcu(&thread->processEntry);
    lock_release(&thread->process->threads.lock);

    rcu_call(&thread->rcu, thread_rcu_free, thread);
}

status_t thread_kernel_create(thread_kernel_entry_t entry, void* arg, thrd_t* out)
{
    if (entry == NULL)
    {
        return ERR(SCHED, INVAL);
    }

    thread_t* thread;
    status_t status = thread_new(&thread, process_get_kernel());
    if (IS_ERR(status))
    {
        return status;
    }

    thread->frame.rip = (uintptr_t)entry;
    thread->frame.rdi = (uintptr_t)arg;
    thread->frame.rbp = thread->kernelStack.top;
    thread->frame.rsp = thread->kernelStack.top;
    thread->frame.cs = GDT_CS_RING0;
    thread->frame.ss = GDT_SS_RING0;
    thread->frame.rflags = RFLAGS_ALWAYS_SET | RFLAGS_INTERRUPT_ENABLE;

    if (out != NULL)
    {
        *out = thread->id;
    }
    sched_submit(thread);
    return OK;
}

void thread_save(thread_t* thread, const interrupt_frame_t* frame)
{
    simd_ctx_save(&thread->simd);

    thread->frame = *frame;
}

void thread_load(thread_t* thread, interrupt_frame_t* frame)
{
    *frame = thread->frame;

    vmm_load(&thread->process->space);
    simd_ctx_load(&thread->simd);
    syscall_ctx_load(&thread->syscall);

    msr_write(MSR_FS_BASE, thread->fsBase);
}

bool thread_is_note_pending(thread_t* thread)
{
    return note_amount(&thread->notes) != 0;
}

status_t thread_send_note(thread_t* thread, const char* string)
{
    status_t status = note_send(&thread->notes, string);
    if (IS_ERR(status))
    {
        return status;
    }

    thread_state_t expected = THREAD_BLOCKED;
    if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
    {
        wait_unblock_thread(thread, EINTR);
    }

    return OK;
}

SYSCALL_DEFINE(SYS_ARCH_CTL, arch_op_t op, uintptr_t addr)
{
    thread_t* thread = thread_current();

    status_t status = OK;
    switch (op)
    {
    case ARCH_SET_FS:
        thread->fsBase = addr;
        msr_write(MSR_FS_BASE, addr);
        break;
    case ARCH_GET_FS:
        status = space_copy_in(&thread->process->space, (void*)addr, &thread->fsBase, sizeof(thread->fsBase));
        break;
    default:
        status = ERR(SCHED, INVAL);
    }

    return status;
}

SYSCALL_DEFINE(SYS_THRD_CURRENT)
{
    *_result = thread_current()->id;
    return OK;
}

SYSCALL_DEFINE(SYS_THRD_CREATE, void* entry, void* arg)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;
    space_t* space = &process->space;

    status_t status = space_check_access(space, entry, sizeof(uint64_t));
    if (IS_ERR(status))
    {
        return status;
    }

    // Dont check arg user space can use it however it wants

    thread_t* newThread;
    status = thread_new(&newThread, process);
    if (IS_ERR(status))
    {
        return status;
    }

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    newThread->frame.rip = (uint64_t)entry;
    newThread->frame.rsp = newThread->userStack.top;
    newThread->frame.rbp = newThread->userStack.top;
    newThread->frame.rdi = (uint64_t)arg;
    newThread->frame.cs = GDT_CS_RING3;
    newThread->frame.ss = GDT_SS_RING3;
    newThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    *_result = newThread->id;
    sched_submit(newThread);
    return OK;
}