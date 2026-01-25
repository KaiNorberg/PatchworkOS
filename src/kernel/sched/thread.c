#include <kernel/cpu/syscall.h>
#include <kernel/sched/thread.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/cache.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <stdlib.h>
#include <string.h>
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
    thread->rcu = (rcu_entry_t){0};
    thread->fsBase = 0;
    memset_s(&thread->frame, sizeof(interrupt_frame_t), 0, sizeof(interrupt_frame_t));
}

static cache_t cache = CACHE_CREATE(cache, "thread", sizeof(thread_t), CACHE_LINE, NULL, NULL);

static uintptr_t thread_id_to_offset(tid_t tid, uint64_t maxPages)
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
            VMM_USER_SPACE_MAX - thread_id_to_offset(thread->id, CONFIG_MAX_USER_STACK_PAGES),
            CONFIG_MAX_USER_STACK_PAGES);
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

void thread_free(thread_t* thread)
{
    lock_acquire(&thread->process->threads.lock);
    thread->process->threads.count--;
    list_remove_rcu(&thread->processEntry);
    lock_release(&thread->process->threads.lock);

    UNREF(thread->process);
    thread->process = NULL;

    simd_ctx_deinit(&thread->simd);

    rcu_call(&thread->rcu, rcu_call_cache_free, thread);
}

status_t thread_kernel_create(tid_t* out, thread_kernel_entry_t entry, void* arg)
{
    if (out == NULL || entry == NULL)
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

    *out = thread->id;
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

uint64_t thread_send_note(thread_t* thread, const char* string)
{
    if (note_send(&thread->notes, string) == _FAIL)
    {
        return _FAIL;
    }

    thread_state_t expected = THREAD_BLOCKED;
    if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
    {
        wait_unblock_thread(thread, EINTR);
    }

    return 0;
}

SYSCALL_DEFINE(SYS_ERRNO, errno_t)
{
    return errno;
}

SYSCALL_DEFINE(SYS_GETTID, tid_t)
{
    return thread_current()->id;
}

status_t thread_copy_from_user(thread_t* thread, void* dest, const void* userSrc, uint64_t length)
{
    if (thread == NULL || dest == NULL || userSrc == NULL || length == 0)
    {
        return ERR(SCHED, INVAL);
    }

    status_t status = space_pin(&thread->process->space, userSrc, length, &thread->userStack);
    if (IS_FAIL(status))
    {
        return status;
    }

    memcpy(dest, userSrc, length);
    space_unpin(&thread->process->space, userSrc, length);
    return OK;
}

status_t thread_copy_to_user(thread_t* thread, void* userDest, const void* src, uint64_t length)
{
    if (thread == NULL || userDest == NULL || src == NULL || length == 0)
    {
        return ERR(SCHED, INVAL);
    }

    status_t status = space_pin(&thread->process->space, userDest, length, &thread->userStack);
    if (IS_FAIL(status))
    {
        return status;
    }

    memcpy(userDest, src, length);
    space_unpin(&thread->process->space, userDest, length);
    return OK;
}

status_t thread_copy_from_user_terminated(thread_t* thread, const void* userArray, const void* terminator,
    uint8_t objectSize, uint64_t maxCount, void** outArray, uint64_t* outCount)
{
    if (thread == NULL || userArray == NULL || terminator == NULL || objectSize == 0 || maxCount == 0 ||
        outArray == NULL)
    {
        return ERR(SCHED, INVAL);
    }

    size_t arraySize;
    status_t status = space_pin_terminated(&arraySize, &thread->process->space, userArray, terminator, objectSize,
        maxCount, &thread->userStack);
    if (IS_FAIL(status))
    {
        return status;
    }

    uint64_t elementCount = arraySize / objectSize;
    uint64_t allocSize = arraySize;

    void* kernelArray = malloc(allocSize);
    if (kernelArray == NULL)
    {
        space_unpin(&thread->process->space, userArray, arraySize);
        return ERR(SCHED, NOMEM);
    }

    memcpy(kernelArray, userArray, arraySize);
    space_unpin(&thread->process->space, userArray, arraySize);

    *outArray = kernelArray;
    if (outCount != NULL)
    {
        *outCount = elementCount - 1;
    }

    return OK;
}

status_t thread_copy_from_user_string(thread_t* thread, char* dest, const char* userSrc, uint64_t size)
{
    if (thread == NULL || dest == NULL || userSrc == NULL || size <= 1)
    {
        return ERR(SCHED, INVAL);
    }

    char terminator = '\0';
    size_t strLength;
    status_t status = space_pin_terminated(&strLength, &thread->process->space, userSrc, &terminator, sizeof(char),
        size - 1, &thread->userStack);
    if (IS_FAIL(status))
    {
        return status;
    }
    dest[size - 1] = '\0';

    memcpy(dest, userSrc, strLength);
    space_unpin(&thread->process->space, userSrc, strLength);
    return OK;
}

status_t thread_copy_from_user_pathname(thread_t* thread, pathname_t* pathname, const char* userPath)
{
    if (thread == NULL || pathname == NULL || userPath == NULL)
    {
        return ERR(SCHED, INVAL);
    }

    char terminator = '\0';
    size_t pathLength;
    status_t status = space_pin_terminated(&pathLength, &thread->process->space, userPath, &terminator, sizeof(char),
        MAX_PATH, &thread->userStack);
    if (IS_FAIL(status))
    {
        return status;
    }

    char copy[MAX_PATH];
    memcpy(copy, userPath, pathLength);
    space_unpin(&thread->process->space, userPath, pathLength);

    status = pathname_init(pathname, copy);
    if (IS_FAIL(status))
    {
        return status;
    }

    return OK;
}

status_t thread_copy_from_user_string_array(thread_t* thread, const char** user, char*** out, uint64_t* outAmount)
{
    if (thread == NULL || user == NULL || out == NULL)
    {
        return ERR(SCHED, INVAL);
    }

    char** copy;
    uint64_t amount;
    char* terminator = NULL;
    status_t status = thread_copy_from_user_terminated(thread, (void*)user, (void*)&terminator, sizeof(char*),
        CONFIG_MAX_ARGC, (void**)&copy, &amount);
    if (IS_FAIL(status))
    {
        return status;
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        char* strCopy;
        uint64_t strLen;
        char strTerminator = '\0';
        status = thread_copy_from_user_terminated(thread, copy[i], &strTerminator, sizeof(char), MAX_PATH,
            (void**)&strCopy, &strLen);
        if (IS_FAIL(status))
        {
            for (uint64_t j = 0; j < i; j++)
            {
                free(copy[j]);
            }
            free((void*)copy);
            return status;
        }

        copy[i] = strCopy;
    }

    *out = copy;
    if (outAmount != NULL)
    {
        *outAmount = amount;
    }

    return OK;
}

status_t thread_load_atomic_from_user(thread_t* thread, atomic_uint64_t* userObj, uint64_t* outValue)
{
    if (thread == NULL || userObj == NULL || outValue == NULL)
    {
        return ERR(SCHED, INVAL);
    }

    status_t status = space_pin(&thread->process->space, userObj, sizeof(atomic_uint64_t), &thread->userStack);
    if (IS_FAIL(status))
    {
        return status;
    }

    *outValue = atomic_load(userObj);
    space_unpin(&thread->process->space, userObj, sizeof(atomic_uint64_t));
    return OK;
}

SYSCALL_DEFINE(SYS_ARCH_PRCTL, uint64_t, arch_prctl_t op, uintptr_t addr)
{
    thread_t* thread = thread_current();

    switch (op)
    {
    case ARCH_SET_FS:
        thread->fsBase = addr;
        msr_write(MSR_FS_BASE, addr);
        return 0;
    case ARCH_GET_FS:
        if (IS_FAIL(thread_copy_to_user(thread, (void*)addr, &thread->fsBase, sizeof(uintptr_t))))
        {
            return _FAIL;
        }
        return 0;
    default:
        errno = EINVAL;
        return _FAIL;
    }
}