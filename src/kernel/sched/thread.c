#include <kernel/sched/thread.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <stdlib.h>
#include <string.h>
#include <sys/list.h>
#include <sys/math.h>

static uintptr_t thread_id_to_offset(tid_t tid, uint64_t maxPages)
{
    return tid * ((maxPages + STACK_POINTER_GUARD_PAGES) * PAGE_SIZE);
}

thread_t* thread_new(process_t* process)
{
    if (process == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    thread_t* thread = malloc(sizeof(thread_t));
    if (thread == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    LOCK_SCOPE(&process->threads.lock);

    if (atomic_load(&process->flags) & PROCESS_DYING)
    {
        errno = EINVAL;
        return NULL;
    }

    thread->process = process;
    list_entry_init(&thread->processEntry);
    thread->id = thread->process->threads.newTid++;
    sched_client_init(&thread->sched);
    atomic_init(&thread->state, THREAD_PARKED);
    thread->error = 0;
    if (stack_pointer_init(&thread->kernelStack,
            VMM_KERNEL_STACKS_MAX - thread_id_to_offset(thread->id, CONFIG_MAX_KERNEL_STACK_PAGES),
            CONFIG_MAX_KERNEL_STACK_PAGES) == ERR)
    {
        free(thread);
        return NULL;
    }
    if (stack_pointer_init(&thread->userStack,
            VMM_USER_SPACE_MAX - thread_id_to_offset(thread->id, CONFIG_MAX_USER_STACK_PAGES),
            CONFIG_MAX_USER_STACK_PAGES) == ERR)
    {
        free(thread);
        return NULL;
    }
    wait_client_init(&thread->wait);
    if (simd_ctx_init(&thread->simd) == ERR)
    {
        free(thread);
        return NULL;
    }
    note_queue_init(&thread->notes);
    syscall_ctx_init(&thread->syscall, &thread->kernelStack);
    perf_thread_ctx_init(&thread->perf);

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));

    REF(process);
    list_push_back(&process->threads.list, &thread->processEntry);
    return thread;
}

tid_t thread_kernel_create(thread_kernel_entry_t entry, void* arg)
{
    if (entry == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    thread_t* thread = thread_new(process_get_kernel());
    if (thread == NULL)
    {
        return ERR;
    }

    thread->frame.rip = (uintptr_t)entry;
    thread->frame.rdi = (uintptr_t)arg;
    thread->frame.rbp = thread->kernelStack.top;
    thread->frame.rsp = thread->kernelStack.top;
    thread->frame.cs = GDT_CS_RING0;
    thread->frame.ss = GDT_SS_RING0;
    thread->frame.rflags = RFLAGS_ALWAYS_SET | RFLAGS_INTERRUPT_ENABLE;

    tid_t volatile tid = thread->id;
    sched_submit(thread);
    return tid;
}

void thread_free(thread_t* thread)
{
    lock_acquire(&thread->process->threads.lock);
    list_remove(&thread->processEntry);
    lock_release(&thread->process->threads.lock);

    UNREF(thread->process);
    thread->process = NULL;

    simd_ctx_deinit(&thread->simd);
    free(thread);
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
}

bool thread_is_note_pending(thread_t* thread)
{
    return note_amount(&thread->notes) != 0;
}

uint64_t thread_send_note(thread_t* thread, const char* string)
{
    if (note_send(&thread->notes, string) == ERR)
    {
        return ERR;
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
    return sched_thread()->id;
}

uint64_t thread_copy_from_user(thread_t* thread, void* dest, const void* userSrc, uint64_t length)
{
    if (thread == NULL || dest == NULL || userSrc == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space_pin(&thread->process->space, userSrc, length, &thread->userStack) == ERR)
    {
        return ERR;
    }

    memcpy(dest, userSrc, length);
    space_unpin(&thread->process->space, userSrc, length);
    return 0;
}

uint64_t thread_copy_to_user(thread_t* thread, void* userDest, const void* src, uint64_t length)
{
    if (thread == NULL || userDest == NULL || src == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space_pin(&thread->process->space, userDest, length, &thread->userStack) == ERR)
    {
        return ERR;
    }

    memcpy(userDest, src, length);
    space_unpin(&thread->process->space, userDest, length);
    return 0;
}

uint64_t thread_copy_from_user_terminated(thread_t* thread, const void* userArray, const void* terminator,
    uint8_t objectSize, uint64_t maxCount, void** outArray, uint64_t* outCount)
{
    if (thread == NULL || userArray == NULL || terminator == NULL || objectSize == 0 || maxCount == 0 ||
        outArray == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t arraySize =
        space_pin_terminated(&thread->process->space, userArray, terminator, objectSize, maxCount, &thread->userStack);
    if (arraySize == ERR)
    {
        return ERR;
    }

    uint64_t elementCount = arraySize / objectSize;
    uint64_t allocSize = arraySize;

    void* kernelArray = malloc(allocSize);
    if (kernelArray == NULL)
    {
        space_unpin(&thread->process->space, userArray, arraySize);
        errno = ENOMEM;
        return ERR;
    }

    memcpy(kernelArray, userArray, arraySize);
    space_unpin(&thread->process->space, userArray, arraySize);

    *outArray = kernelArray;
    if (outCount != NULL)
    {
        *outCount = elementCount - 1;
    }

    return 0;
}

uint64_t thread_copy_from_user_string(thread_t* thread, char* dest, const char* userSrc, uint64_t size)
{
    if (thread == NULL || dest == NULL || userSrc == NULL || size <= 1)
    {
        errno = EINVAL;
        return ERR;
    }

    char terminator = '\0';
    uint64_t strLength =
        space_pin_terminated(&thread->process->space, userSrc, &terminator, sizeof(char), size - 1, &thread->userStack);
    if (strLength == ERR)
    {
        return ERR;
    }
    dest[size - 1] = '\0';

    memcpy(dest, userSrc, strLength);
    space_unpin(&thread->process->space, userSrc, strLength);
    return 0;
}

uint64_t thread_copy_from_user_pathname(thread_t* thread, pathname_t* pathname, const char* userPath)
{
    if (thread == NULL || pathname == NULL || userPath == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    char terminator = '\0';
    uint64_t pathLength = space_pin_terminated(&thread->process->space, userPath, &terminator, sizeof(char), MAX_PATH,
        &thread->userStack);
    if (pathLength == ERR)
    {
        return ERR;
    }

    char copy[MAX_PATH];
    memcpy(copy, userPath, pathLength);
    space_unpin(&thread->process->space, userPath, pathLength);

    if (pathname_init(pathname, copy) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t thread_copy_from_user_string_array(thread_t* thread, const char** user, char*** out, uint64_t* outAmount)
{
    if (thread == NULL || user == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    char** copy;
    uint64_t amount;
    char* terminator = NULL;
    if (thread_copy_from_user_terminated(thread, (void*)user, (void*)&terminator, sizeof(char*), CONFIG_MAX_ARGC,
            (void**)&copy, &amount) == ERR)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        char* strCopy;
        uint64_t strLen;
        char strTerminator = '\0';
        if (thread_copy_from_user_terminated(thread, copy[i], &strTerminator, sizeof(char), MAX_PATH, (void**)&strCopy,
                &strLen) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                free(copy[j]);
            }
            free((void*)copy);
            return ERR;
        }

        copy[i] = strCopy;
    }

    *out = copy;
    if (outAmount != NULL)
    {
        *outAmount = amount;
    }

    return 0;
}

uint64_t thread_load_atomic_from_user(thread_t* thread, atomic_uint64_t* userObj, uint64_t* outValue)
{
    if (thread == NULL || userObj == NULL || outValue == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space_pin(&thread->process->space, userObj, sizeof(atomic_uint64_t), &thread->userStack) == ERR)
    {
        return ERR;
    }

    *outValue = atomic_load(userObj);
    space_unpin(&thread->process->space, userObj, sizeof(atomic_uint64_t));
    return 0;
}
