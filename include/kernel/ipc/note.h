#pragma once

#include <kernel/config.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/sync/lock.h>

#include <kernel/utils/map.h>
#include <sys/io.h>
#include <sys/proc.h>

typedef struct cpu cpu_t;

/**
 * @brief Signal style inter-process communication.
 * @defgroup kernel_ipc_note Notes
 * @ingroup kernel_ipc
 *
 * Notes are exposed in the `/proc/[pid]/note` file and are used for inter-process communication (IPC) similarly to
 * signals in Unix-like operating systems. However, instead of being limited to a predefined set of integer values,
 * notes can send arbitrary strings.
 *
 * ## Using Notes
 *
 * To send or receive notes, each process exposes a set of files in its `/proc/[pid]` directory.
 *
 * @see kernel_proc_process
 *
 * ## Receiving Notes
 *
 * In the kernel, notes are received and handled in the `note_handle_pending()` function, which is called from an
 * interrupt context.
 *
 * From the perspective of user space, a thread will be interrupted the next time a kernel to user boundary is crossed,
 * asuming there is a note pending.
 *
 * The interruption works by having the kernel save the current interrupt frame of the thread and replacing it with a
 * new frame that calls the note handler function registered using `notify()`. During the handling of the note, no
 * further notes will be delivered to the thread.
 *
 * Later, when the note handler function calls `noted()`, the kernel will restore the saved interrupt frame and continue
 * execution from where it left off as if nothing happened. Alternatively, the note handler can choose to exit the
 * thread. If no handler is registered, the thread is killed.
 *
 * ## System Notes
 *
 * Certain notes will cause the kernel to take special actions and for the sake of consistency, we define
 * some notes that all user processes are expected to handle in a standardized way.
 *
 * Any such notes are written as a word optionally followed by additional data. For example, a "terminate" note could be
 * send as is or with a reason string like "terminate due to low memory".
 *
 * ### "kill" (SIGKILL)
 *
 * When a thread receives this note, it will immediately transition to the `THREAD_DYING` state, causing the scheduler
 * to kill and free the thread. User space will never see this note.
 *
 * ### "divbyzero" (SIGFPE)
 *
 * The thread attempted to divide by zero.
 *
 * ### "illegal" (SIGILL)
 *
 * The thread attempted to execute an illegal instruction.
 *
 * ### "interrupt" (SIGINT)
 *
 * Indicates an interrupt from the user, typically initiated by pressing `Ctrl+C` in a terminal.
 *
 * ### "pagefault" (SIGSEGV)
 *
 * Indicates that the thread made an invalid memory access, such as dereferencing a null or invalid pointer.
 *
 * ### "segfault" (SIGSEGV)
 *
 * Indicates that the thread made an invalid memory access, that dident cause a page fault, such as executing a invalid
 * instruction.
 *
 * ### "terminate" (SIGTERM)
 *
 * Indicates that the process should perform any necessary cleanup and exit gracefully.
 *
 * ## User-defined Notes
 *
 * All system notes will always start with a single word consisting of only lowercase letters.
 *
 * If a program wishes to define its own notes, it is best practice to avoid using such words to prevent conflicts with
 * future system notes. For example, here are some safe to use user-defined notes:
 * - "user_note ..."
 * - "UserNote ..."
 * - "USER-NOTE ..."
 * - "1usernote ..."
 *
 * @{
 */

/**
 * @brief Maximum size of a notes buffer.
 */
#define NOTE_MAX 256

/**
 * @brief Note queue flags.
 * @enum note_queue_flag_t
 *
 * Its vital that a certain special notes get handled, even if we run out of memory. Since these notes have a predefined
 * value and we dont care if they get sent multiple times, we can simplify the system such that when the note queue
 * receives a special note instead of pushing it to the queue we just set the corresponding flag.
 */
typedef enum
{
    NOTE_QUEUE_NONE = 0,
    NOTE_QUEUE_RECEIVED_KILL = 1 << 0,
    NOTE_QUEUE_HANDLING = 1 << 1, ///< User space is currently handling a note.
} note_queue_flag_t;

/**
 * @brief Note structure.
 * @struct note_t
 */
typedef struct note
{
    char buffer[NOTE_MAX];
    pid_t sender;
} note_t;

/**
 * @brief Per-process note handler.
 * @struct note_handler_t
 */
typedef struct
{
    note_func_t func;
    lock_t lock;
} note_handler_t;

/**
 * @brief Per-thread note queue.
 * @struct note_queue_t
 */
typedef struct
{
    note_t notes[CONFIG_MAX_NOTES];
    size_t readIndex;
    size_t writeIndex;
    uint64_t length;
    note_queue_flag_t flags;
    interrupt_frame_t noteFrame; ///< The interrupt frame to return to after handling a note.
    lock_t lock;
} note_queue_t;

/**
 * @brief Initialize a note handler.
 *
 * @param handler The handler to initialize.
 */
void note_handler_init(note_handler_t* handler);

/**
 * @brief Initialize a note queue.
 *
 * @param queue The queue to initialize.
 */
void note_queue_init(note_queue_t* queue);

/**
 * @brief The amount of pending notes in a note queue, including special notes.
 *
 * @param queue The queue to query.
 * @return The amount of pending notes.
 */
uint64_t note_amount(note_queue_t* queue);

/**
 * @brief Write a note to a note queue.
 *
 * @param queue The destination queue.
 * @param string The string to write, should be a null-terminated string.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t note_send(note_queue_t* queue, const char* string);

/**
 * @brief Handle pending notes for the current thread.
 *
 * Should only be called from an interrupt context.
 *
 * If the frame is not from user space, this function will return immediately.
 *
 * @param frame The interrupt frame.
 * @return `true` if a note was handled, `false` otherwise.
 */
bool note_handle_pending(interrupt_frame_t* frame);

/** @} */
