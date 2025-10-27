#pragma once

#include "config.h"
#include "cpu/interrupt.h"
#include "sync/lock.h"

#include <sys/io.h>
#include <sys/proc.h>

typedef struct cpu cpu_t;

/**
 * @brief Signal style inter-process communication.
 * @defgroup kernel_ipc_note Notes
 * @ingroup kernel_ipc
 *
 * Notes are exposed in the `/proc/[pid]/note` file. Notes are used for inter-process communication (IPC) in a way
 * similar to signals in Unix-like operating systems. However, they allow for a buffer of data to be sent instead of
 * just an integer. In theory anything could be sent as a note.
 *
 * ## Using Notes
 *
 * Notes are sent by writing to the `/proc/[pid]/note` file of the target process. The data written to the file is sent
 * as a note to one of the threads in the target process.
 *
 * ## Receiving Notes
 *
 * TODO: Receiving notes, software interrupts, etc.
 *
 * ## Special Notes
 *
 * Certain notes will cause the kernel to take special actions and, for the sake of concistency, we define some notes
 * that all user processes should handle in a standard way. The values for these notes are intended to mirror UNIX
 * signals where applicable. Below is a list of all of these special notes:
 * - "kill": When the kernel receives this note, it will immediately terminate the target thread's process. User space
 * will never see this signal. (UNIX SIGKILL)
 *
 *
 *
 * @{
 */

/**
 * @brief Maximum size of a notes buffer.
 */
#define NOTE_MAX_BUFFER 64

/**
 * @brief Note queue flags.
 * @enum note_queue_flag_t
 */
typedef enum
{
    NOTE_QUEUE_NONE = 0,
    /**
     * Its vital that a kill note gets handled, even if we run out of memory. Since these notes have a predefined value
     * and we dont care if they get sent multiple times, we can simplify the system such that when the note queue
     * recieves a kill note instead of pushing it to the queue we just set the corresponding flag.
     *
     * The thread will never know the difference.
     */
    NOTE_QUEUE_RECIEVED_KILL = 1 << 0,
} note_queue_flag_t;

/**
 * @brief Note structure.
 * @struct note_t
 */
typedef struct note
{
    uint8_t buffer[NOTE_MAX_BUFFER];
    uint16_t length;
    pid_t sender;
} note_t;

/**
 * @brief Per-thread note queue.
 * @struct note_queue_t
 */
typedef struct
{
    note_t notes[CONFIG_MAX_NOTES];
    uint64_t readIndex;
    uint64_t writeIndex;
    uint64_t length;
    note_queue_flag_t flags;
    lock_t lock;
} note_queue_t;

/**
 * @brief Initialize a note queue.
 *
 * @param queue The queue to initialize.
 */
void note_queue_init(note_queue_t* queue);

/**
 * @brief Get the length of a note queue.
 *
 * @param queue The queue to query.
 * @return The length of the queue.
 */
uint64_t note_queue_length(note_queue_t* queue);

/**
 * @brief Write a note to a note queue.
 *
 * @param queue The destination queue.
 * @param buffer The buffer to write.
 * @param count The number of bytes to write.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t note_queue_write(note_queue_t* queue, const void* buffer, uint64_t count);

/**
 * @brief Note interrupt handler.
 *
 * @param frame The current interrupt frame.
 * @param self The currently running cpu.
 */
void note_interrupt_handler(interrupt_frame_t* frame, cpu_t* self);

/** @} */
