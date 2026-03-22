#ifndef _SYS_NOTE_H
#define _SYS_NOTE_H 1

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/NULL.h"
#include "_libstd/PAGE_SIZE.h"
#include "_libstd/clock_t.h"
#include "_libstd/config.h"

/**
 * @brief Notes.
 * @ingroup libstd
 * @defgroup libstd_sys_note Notes
 *
 * @{
 */

/**
 * @brief Note handler function type.
 */
typedef void (*note_func_t)(char* note);

/**
 * @brief System call that sets the handler to be called when a note is received.
 *
 * A note handler must either exit the thread or call `note_done()`.
 *
 * If no handler is registered, the thread is killed when a note is received.
 *
 * @warning It is preferred to use `note_at()` instead of this function as using this will prevent the standard library
 * from handling notes.
 *
 * @see kernel_ipc_note
 *
 * @param handler The handler function to be called on notes, can be `NULL` to unregister the current handler.
 * @return An appropriate status value.
 */
static inline status_t note_set(note_func_t handler)
{
    return syscall1(SYS_NOTE_SET, NULL, (uintptr_t)handler);
}

/**
 * @brief System call that notifies the kernel that the current note has been handled.
 *
 * Should only be called from within a handler registered with `note_set()` but NOT with `note_at()`.
 *
 * If a note is not currently being handled, the thread is killed.
 *
 * @see kernel_ipc_note
 *
 * @return Never returns, instead resumes execution of the thread where it left off before the note was delivered.
 */
static inline _NORETURN void note_done(void)
{
    syscall0(SYS_NOTE_DONE, NULL);
    __builtin_unreachable();
}

/**
 * @brief Action type for note_at().
 * @enum note_act_t
 */
typedef enum
{
    NOTE_ADD = 0,
    NOTE_REMOVE = 1
} note_act_t;

/**
 * @brief Adds or removes a handler to be called in user space when a note is received.
 *
 * @param handler The handler function to be modified.
 * @param action The action to perform.
 * @return An appropriate status value.
 */
status_t note_at(note_func_t handler, note_act_t action);

/**
 * @brief Helper for comparing the first word of a string.
 *
 * @param string The string.s
 * @param word The word to compare against.
 * @return On match, returns `0`. On mismatch, returns a non-zero value.
 */
int64_t wordcmp(const char* string, const char* word);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
