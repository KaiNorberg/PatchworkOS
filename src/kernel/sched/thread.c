#include <kernel/sched/thread.h>

#include <kernel/cpu/gdt.h>
#include <kernel/cpu/smp.h>
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

static thread_t bootThread;
static bool bootThreadInitalized = false;

static uintptr_t thread_id_to_offset(tid_t tid, uint64_t maxPages)
{
    return tid * ((maxPages + STACK_POINTER_GUARD_PAGES) * PAGE_SIZE);
}

static uint64_t thread_init(thread_t* thread, process_t* process)
{
    if (process == NULL)
    {
        return ERR;
    }

    list_entry_init(&thread->entry);
    thread->process = REF(process);
    list_entry_init(&thread->processEntry);
    thread->id = thread->process->threads.newTid++;
    sched_thread_ctx_init(&thread->sched);
    atomic_init(&thread->state, THREAD_PARKED);
    thread->error = 0;
    if (stack_pointer_init(&thread->kernelStack,
            VMM_KERNEL_STACKS_MAX - thread_id_to_offset(thread->id, CONFIG_MAX_KERNEL_STACK_PAGES),
            CONFIG_MAX_KERNEL_STACK_PAGES) == ERR)
    {
        DEREF(process);
        return ERR;
    }
    if (stack_pointer_init(&thread->userStack,
            VMM_USER_SPACE_MAX - thread_id_to_offset(thread->id, CONFIG_MAX_USER_STACK_PAGES),
            CONFIG_MAX_USER_STACK_PAGES) == ERR)
    {
        DEREF(process);
        return ERR;
    }
    wait_thread_ctx_init(&thread->wait);
    if (simd_ctx_init(&thread->simd) == ERR)
    {
        DEREF(process);
        return ERR;
    }
    note_queue_init(&thread->notes);
    syscall_ctx_init(&thread->syscall, &thread->kernelStack);

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));

    lock_acquire(&process->threads.lock);
    list_push(&process->threads.list, &thread->processEntry);
    lock_release(&process->threads.lock);

    LOG_DEBUG("created tid=%d pid=%d\n", thread->id, process->id);
    return 0;
}

thread_t* thread_new(process_t* process)
{
    if (process == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (atomic_load(&process->isDying))
    {
        errno = EINVAL;
        return NULL;
    }

    thread_t* thread = malloc(sizeof(thread_t));
    if (thread == NULL)
    {
        return NULL;
    }
    if (thread_init(thread, process) == ERR)
    {
        free(thread);
        return NULL;
    }
    return thread;
}

void thread_free(thread_t* thread)
{
    LOG_DEBUG("freeing tid=%d pid=%d\n", thread->id, thread->process->id);

    process_t* process = thread->process;
    assert(process != NULL);

    lock_acquire(&process->threads.lock);
    list_remove(&process->threads.list, &thread->processEntry);
    lock_release(&process->threads.lock);

    DEREF(process);
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

    space_load(&thread->process->space);
    simd_ctx_load(&thread->simd);
    syscall_ctx_load(&thread->syscall);
}

void thread_get_interrupt_frame(thread_t* thread, interrupt_frame_t* frame)
{
    *frame = thread->frame;
}

bool thread_is_note_pending(thread_t* thread)
{
    return note_queue_length(&thread->notes) != 0;
}

uint64_t thread_send_note(thread_t* thread, const void* buffer, uint64_t count)
{
    if (note_queue_write(&thread->notes, buffer, count) == ERR)
    {
        return ERR;
    }

    thread_state_t expected = THREAD_BLOCKED;
    if (atomic_compare_exchange_strong(&thread->state, &expected, THREAD_UNBLOCKING))
    {
        LOG_DEBUG("notifying thread tid=%d pid=%d of pending note\n", thread->id, thread->process->id);
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

thread_t* thread_get_boot(void)
{
    if (!bootThreadInitalized)
    {
        if (thread_init(&bootThread, process_get_kernel()) == ERR)
        {
            panic(NULL, "Failed to initialize boot thread");
        }
        LOG_INFO("boot thread initialized with pid=%d tid=%d\n", bootThread.process->id, bootThread.id);
        bootThreadInitalized = true;
    }
    return &bootThread;
}

uint64_t thread_handle_page_fault(const interrupt_frame_t* frame)
{
    thread_t* thread = sched_thread_unsafe();
    if (thread == NULL)
    {
        return ERR;
    }
    uintptr_t faultAddr = (uintptr_t)cr2_read();

    if (frame->errorCode & PAGE_FAULT_PRESENT)
    {
        errno = EFAULT;
        return ERR;
    }

    uintptr_t alignedFaultAddr = ROUND_DOWN(faultAddr, PAGE_SIZE);
    if (stack_pointer_is_in_stack(&thread->userStack, alignedFaultAddr, 1))
    {
        if (vmm_alloc(&thread->process->space, (void*)alignedFaultAddr, PAGE_SIZE, PML_WRITE | PML_PRESENT | PML_USER,
                VMM_ALLOC_FAIL_IF_MAPPED) == NULL)
        {
            if (errno == EEXIST) // Race condition, another CPU mapped the page.
            {
                return 0;
            }

            return ERR;
        }
        memset((void*)alignedFaultAddr, 0, PAGE_SIZE);
        return 0;
    }

    if (INTERRUPT_FRAME_IN_USER_SPACE(frame))
    {
        errno = EFAULT;
        return ERR;
    }

    if (stack_pointer_is_in_stack(&thread->kernelStack, alignedFaultAddr, 1))
    {
        if (vmm_alloc(&thread->process->space, (void*)alignedFaultAddr, PAGE_SIZE, PML_WRITE | PML_PRESENT,
                VMM_ALLOC_FAIL_IF_MAPPED) == NULL)
        {
            if (errno == EEXIST) // Race condition, another CPU mapped the page.
            {
                return 0;
            }
            return ERR;
        }
        memset((void*)alignedFaultAddr, 0, PAGE_SIZE);
        return 0;
    }

    errno = EFAULT;
    return ERR;
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

uint64_t thread_copy_to_user(thread_t* thread, void* dest, const void* userSrc, uint64_t length)
{
    if (thread == NULL || dest == NULL || userSrc == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space_pin(&thread->process->space, dest, length, &thread->userStack) == ERR)
    {
        return ERR;
    }

    memcpy(dest, userSrc, length);
    space_unpin(&thread->process->space, dest, length);
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
    uint64_t allocSize = (elementCount + 1) * objectSize; // +1 for terminator

    void* kernelArray = malloc(allocSize);
    if (kernelArray == NULL)
    {
        space_unpin(&thread->process->space, userArray, arraySize);
        errno = ENOMEM;
        return ERR;
    }

    memcpy(kernelArray, userArray, arraySize);
    memcpy((uint8_t*)kernelArray + arraySize, terminator, objectSize); // Add terminator
    space_unpin(&thread->process->space, userArray, arraySize);

    *outArray = kernelArray;
    if (outCount != NULL)
    {
        *outCount = elementCount;
    }

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
    copy[pathLength] = '\0';
    space_unpin(&thread->process->space, userPath, pathLength);

    if (pathname_init(pathname, copy) == ERR)
    {
        return ERR;
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
