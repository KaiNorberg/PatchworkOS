#pragma once

#include "config.h"
#include "cpu/interrupt.h"
#include "sync/lock.h"

#include <sys/io.h>
#include <sys/proc.h>

typedef struct cpu cpu_t;

/**
 * @brief Signals/Notes.
 * @defgroup kernel_ipc_note Signals/Notes
 * @ingroup kernel_ipc
 *
 * This is effectively our "signal" system but inspired by plan9's note system.
 *
 * @{
 */

/**
 * @brief Note flags.
 * @enum note_flags_t
 */
typedef enum
{
    NOTE_NONE = (1 << 0),
    NOTE_CRITICAL = (1 << 1) ///< Critical notes can overwrite non critical notes when the queue is full.
} note_flags_t;

/**
 * @brief Note structure.
 * @struct note_t
 */
typedef struct note
{
    char message[MAX_PATH];
    pid_t sender;
    note_flags_t flags;
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
 * @brief Push a note to a queue.
 *
 * @param queue The destination queue.
 * @param message The string of text to send to the thread, does not need to be NULL-terminated.
 * @param length The length of the string, must be less than `MAX_PATH - 1`.
 * @param flags The flags for the note.
 * @return On success, returns 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t note_queue_push(note_queue_t* queue, const void* message, uint64_t length, note_flags_t flags);

/**
 * @brief Dispatches received notes.
 *
 * @param frame The current interrupt frame.
 * @param self The currently running cpu.
 */
void note_dispatch(interrupt_frame_t* frame, cpu_t* self);

/** @} */
