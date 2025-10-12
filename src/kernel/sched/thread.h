#pragma once

#include "config.h"
#include "cpu/simd.h"
#include "cpu/syscalls.h"
#include "cpu/trap.h"
#include "ipc/note.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/wait.h"

#include <errno.h>
#include <sys/list.h>
#include <sys/proc.h>

/**
 * @brief Thread of execution.
 * @ingroup kernel_sched
 * @defgroup kernel_sched_thread Threads
 * @{
 */

/**
 * @brief Magic number to check for kernel stack overflow.
 * @def THREAD_CANARY
 *
 * The `THREAD_CANARY` constant is a magic number stored att the bottom of a threads kernel stack, if the kernel stack
 * were to overflow this value would be modified, therefor we check the value of the canary every now and then.
 *
 * Note that this kind of check is not fool proof, for example if a very large stack overflow were to occur we would get
 * unpredictable behaviour as this would result in other modifications to the `thread_t` structure, however adding the
 * canary makes debugging far easier if a stack overflow were to occur and should catch the majority of overflows. If an
 * overflow in the kernel stack does occur increasing the value of `CONFIG_MAX_KERNEL_STACK` should fix the problem.
 *
 */
#define THREAD_CANARY 0x1A4DA90211FFC68CULL

/**
 * @brief Thread state enum.
 * @enum thread_state_t
 *
 */
typedef enum
{
    THREAD_PARKED = 1 << 0,    ///< Is doing nothing, not in a queue, not blocking, think of it as "other".
    THREAD_READY = 1 << 1,     ///< Is ready to run and waiting to be scheduled.
    THREAD_RUNNING = 1 << 2,   ///< Is currently running on a cpu.
    THREAD_ZOMBIE = 1 << 3,    ///< Has exited and is waiting to be freed and might still be executing.
    THREAD_PRE_BLOCK = 1 << 4, ///< Has started the process of blocking but has not yet been given to a owner cpu.
    THREAD_BLOCKED = 1 << 5,   ///< Is blocking and waiting in one or multiple wait queues.
    THREAD_UNBLOCKING =
        1 << 6, ///< Has started unblocking, used to prevent the same thread being unblocked multiple times.
} thread_state_t;

/**
 * @brief The size of a threads kernel stack.
 * @def THREAD_KERNEL_STACK_SIZE
 *
 * When debugging we need a bigger kernel stack as we are not using optimizations or simply becouse tests can be very memory intensive, especially the aml tests. So when in debug or testing mode we use a bigger kernel stack.
 */
#if !defined(NDEBUG) || defined(TESTING)
#define THREAD_KERNEL_STACK_SIZE (CONFIG_KERNEL_STACK * 8)
#else
#define THREAD_KERNEL_STACK_SIZE (CONFIG_KERNEL_STACK )
#endif

/**
 * @brief Thread of execution structure.
 * @struct thread_t
 *
 * A `thread_t` represents an independent thread of execution within a `process_t`.
 *
 */
typedef struct thread
{
    /**
     * @brief The list entry used by for example the scheduler and wait system.
     */
    list_entry_t entry;
    /**
     * @brief The parent process.
     */
    process_t* process;
    /**
     * @brief The list entry used by the parent process.
     */
    list_entry_t processEntry;
    /**
     * @brief The thread id, unique within a `process_t`.
     */
    tid_t id;
    /**
     * @brief The current state of the thread, used to prevent race conditions and make debugging easier.
     */
    _Atomic(thread_state_t) state;
    /**
     * @brief The last error that occurred while the thread was running, specified using errno codes.
     */
    errno_t error;
    /**
     * @brief The threads sched context, for more info check the type description.
     */
    sched_thread_ctx_t sched;
    /**
     * @brief The threads wait context, for more info check the type description.
     */
    wait_thread_ctx_t wait;
    /**
     * @brief The threads simd context, for more info check the type description.
     */
    simd_ctx_t simd;
    /**
     * @brief The threads note queue context, for more info check the type description.
     */
    note_queue_t notes;
    /**
     * @brief The threads syscall context, for more info check the type description.
     */
    syscall_ctx_t syscall;
    /**
     * @brief The threads trap frame is used to save the values in the CPU registers such that the
     * scheduler can continue executing the thread later on.
     */
    trap_frame_t trapFrame;
    /**
     * @brief The threads canary, for more info see `THREAD_CANARY`.
     */
    uint64_t canary;
    /**
     * @brief The threads kernel stack, stored as part of the `thread_t` structure in
     * order to avoid an additional allocation when allocating a new thread.
     */
    uint8_t kernelStack[THREAD_KERNEL_STACK_SIZE];
} thread_t;

/**
 * @brief Retrieves the top of a threads kernel stack.
 * @def THREAD_KERNEL_STACK_TOP
 *
 * Note that in x86 the push operation moves the stack pointer first and then writes to the location of the stack
 * pointer, that means that even if this address is not inclusive the stack pointer of a thread should be initalized to
 * the result of the `THREAD_KERNEL_STACK_TOP()` macro.
 *
 * @return The address of the top of the kernel stack, this address is not inclusive.
 */
#define THREAD_KERNEL_STACK_TOP(thread) ((uintptr_t)thread->kernelStack + THREAD_KERNEL_STACK_SIZE)

/**
 * @brief Retrieves the bottom of a threads kernel stack.
 * @def THREAD_KERNEL_STACK_BOTTOM
 *
 * @return The address of the bottom of the kernel stack, this address is inclusive.
 */
#define THREAD_KERNEL_STACK_BOTTOM(thread) ((uintptr_t)thread->kernelStack)

/**
 * @brief Retrieves the top of a threads kernel stack.
 *
 * Used in `start.s`, since we cant use the `THREAD_KERNEL_STACK_TOP` macro in assembly.
 *
 * @see THREAD_KERNEL_STACK_TOP()
 *
 * @param thread The thread to query.
 * @return The address of the top of the kernel stack, this address is not inclusive.
 */
uint64_t thread_get_kernel_stack_top(thread_t* thread);

/**
 * @brief Creates a new thread structure.
 *
 * Does not push the created thread to the scheduler or similar, merely handling allocation and initialization.
 *
 * @param process The parent process that the thread will execute within.
 * @param entry The inital value of the threads rip register, defines where the thread will start executing code.
 * @return On success, returns the newly created thread. On failure, returns `NULL`.
 */
thread_t* thread_new(process_t* process, void* entry);

/**
 * @brief Frees a thread structure.
 *
 * @param thread The thread to be freed.
 */
void thread_free(thread_t* thread);

/**
 * @brief Signals to a thread that it is dying.
 *
 * Does not perform free the thread and the thread will continue executing as a zombie after this function.
 *
 * @param thread The thread to be killed.
 */
void thread_kill(thread_t* thread);

/**
 * @brief Save state to thread.
 *
 * @param thread The destination thread where the state will be saved.
 * @param trapFrame The source trapframe storing register state.
 */
void thread_save(thread_t* thread, const trap_frame_t* trapFrame);

/**
 * @brief Load state from thread.
 *
 * @param thread The source thread to load state from.
 * @param trapFrame The destination trap frame to load register state.
 */
void thread_load(thread_t* thread, trap_frame_t* trapFrame);

/**
 * @brief Check if a thread has a note pending.
 *
 * @param thread The thread to query.
 * @return True if there is a note pending, false otherwise.
 */
bool thread_is_note_pending(thread_t* thread);

/**
 * @brief Send a note to a thread.
 *
 * This function should always be used over the `note_queue_push()` function, as it performs additional checks, like
 * deciding how critical the sent note is and unblocking the thread to notify it of the received note.
 *
 * @param thread The destination thread.
 * @param message The string of text to send to the thread, does not need to be NULL-terminated.
 * @param length The length of the string.
 * @return On success, returns 0. On failure, returns `ERR`.
 */
uint64_t thread_send_note(thread_t* thread, const void* message, uint64_t length);

/**
 * @brief Retrieves the boot thread.
 *
 * The boot thread is the first thread created by the kernel. After boot it becomes the idle thread of the
 * bootstrap CPU. Is initialized lazily on the first call to this function, which should happen during early boot.
 *
 * Will never return `NULL`.
 *
 * @return The boot thread.
 */
thread_t* thread_get_boot(void);

/** @} */
